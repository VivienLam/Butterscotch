// Heap wrappers (malloc, realloc, calloc are linked through these: -Wl,--wrap, cmake/n3ds.cmake).
//
// Low memory: the renderer keeps the atlas pages' file data in the heap as a cache and grows it while the heap has
// room. Anything else that then runs out (the VM, stb_ds arrays, which don't check) used to crash: on a failed
// allocation, the main thread first asks the registered reclaimer to free cached data, and tries again.
//
// Debug build option N3DS_POISON_MALLOC: new heap memory is filled with a pattern instead of whatever the allocator
// hands back. On hardware, reused heap memory holds old data while an emulator's or a PC's fresh memory is mostly
// zero, so a field the code reads before setting it can behave on one and not the other; this makes the emulator
// behave like the worst case.

#include "n3ds_lowmem.h"

#include <3ds.h>
#include <malloc.h>
#include <stddef.h>
#include <string.h>

#define N3DS_POISON_BYTE 0xA5

void* __real_malloc(size_t size);
void* __real_realloc(void* ptr, size_t size);
void* __real_calloc(size_t count, size_t size);

static N3DSLowMemReclaim gReclaim = NULL;
static bool gReclaiming = false;
static uint32_t gReclaimedBytes = 0, gReclaimCount = 0;

void N3DSLowMem_setReclaim(N3DSLowMemReclaim reclaim) {
    gReclaim = reclaim;
}

void N3DSLowMem_stats(uint32_t* reclaimedBytes, uint32_t* reclaimCount) {
    *reclaimedBytes = gReclaimedBytes;
    *reclaimCount = gReclaimCount;
}

// One more round of freeing cached data, if this is the main thread (the reclaimer isn't thread safe; the loader
// thread handles its own failures) and not already inside the reclaimer. False: nothing left to free.
static bool reclaimFor(size_t size) {
    if (gReclaim == NULL || gReclaiming || threadGetCurrent() != NULL) return false;
    gReclaiming = true;
    size_t freed = gReclaim(size);
    gReclaiming = false;
    if (freed == 0) return false;
    gReclaimedBytes += (uint32_t) freed;
    gReclaimCount++;
    return true;
}

void* __wrap_malloc(size_t size) {
    void* p = __real_malloc(size);
    while (p == NULL && size > 0 && reclaimFor(size)) p = __real_malloc(size);
#ifdef N3DS_POISON_MALLOC
    if (p != NULL) memset(p, N3DS_POISON_BYTE, size);
#endif
    return p;
}

void* __wrap_realloc(void* ptr, size_t size) {
#ifdef N3DS_POISON_MALLOC
    size_t old = ptr != NULL ? malloc_usable_size(ptr) : 0;
#endif
    void* p = __real_realloc(ptr, size);
    while (p == NULL && size > 0 && reclaimFor(size)) p = __real_realloc(ptr, size);
#ifdef N3DS_POISON_MALLOC
    if (p != NULL && size > old) memset((char*) p + old, N3DS_POISON_BYTE, size - old);
#endif
    return p;
}

void* __wrap_calloc(size_t count, size_t size) {
    void* p = __real_calloc(count, size);
    while (p == NULL && count > 0 && size > 0 && reclaimFor(count * size)) p = __real_calloc(count, size);
    return p;
}
