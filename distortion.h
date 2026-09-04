#pragma once

#include <stdint.h>

// Distortion stage applied to the final mixed sample in audio_task.cpp.
//
// Pick exactly ONE of the following (or leave all commented out for a
// clean passthrough) and recompile. Only the selected algorithm's code is
// compiled in via #ifdef in distortion.cpp — switching flavors never
// uploads unused DSP code onto the FPU-less C3.
// #define DISTORTION_HARD_CLIP
// #define DISTORTION_SOFT_CLIP
// #define DISTORTION_FOLDBACK
#define DISTORTION_BITCRUSH

// Integer gain applied before clipping/folding (not used by bitcrush).
// Higher = more aggressive distortion.
#define DISTORTION_DRIVE_GAIN 6

// DISTORTION_FOLDBACK: samples beyond +-this reflect back down/up instead
// of clipping.
#define DISTORTION_FOLD_THRESHOLD 12000

// DISTORTION_BITCRUSH: number of low bits zeroed out. 0 = no effect,
// 12 = extremely crushed.
#define DISTORTION_BITCRUSH_BITS 8

// Applies the selected distortion to one sample (a no-op if none of the
// DISTORTION_* macros above is defined).
int16_t applyDistortion(int16_t sample);
