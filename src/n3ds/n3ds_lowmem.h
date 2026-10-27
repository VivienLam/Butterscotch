#pragma once

#include <stddef.h>
#include <stdint.h>

// Frees cached data to make room for an allocation of `needed` bytes; returns how much it freed (0: nothing left).
typedef size_t (*N3DSLowMemReclaim)(size_t needed);

void N3DSLowMem_setReclaim(N3DSLowMemReclaim reclaim);
void N3DSLowMem_stats(uint32_t* reclaimedBytes, uint32_t* reclaimCount);
