#pragma once

#include "../audio_system.h"

typedef struct N3DSAudioSystem N3DSAudioSystem;

N3DSAudioSystem* N3DSAudioSystem_create(void);
void N3DSAudioSystem_getCacheStats(
    AudioSystem* base,
    uint32_t* outCachedSounds,
    uint32_t* outTotalSounds,
    uint32_t* outCachedBytes,
    uint32_t* outCacheLimitBytes
);

#ifndef RUNNER_DEFINED
#define RUNNER_DEFINED
typedef struct Runner Runner;
#endif
// Loads the sounds a room is likely to need (call when the room changes).
void N3DSAudio_prewarmRoom(AudioSystem* audio, Runner* runner);
// Keeps the music streams fed while a long frame runs (called from the VM's periodic hook).
void N3DSAudio_pumpStreams(AudioSystem* audio);
// Sound effects started so far (perf log).
uint32_t N3DSAudio_effectsStarted(AudioSystem* audio);
