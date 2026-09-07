#pragma once

#include <stdint.h>

// Distortion applied in audio_task.cpp, either per voice or to the mixed
// sample (see DISTORTION_STAGE_* below).
//
// Pick exactly ONE of the following (or leave all commented out for a
// clean passthrough) and recompile. Only the selected algorithm's code is
// compiled in via #ifdef in distortion.cpp — switching flavors never
// uploads unused DSP code onto the FPU-less C3.
// #define DISTORTION_HARD_CLIP
// #define DISTORTION_SOFT_CLIP
// #define DISTORTION_FOLDBACK
#define DISTORTION_BITCRUSH

// Set when any flavor above is selected, so callers can skip the call
// entirely (rather than call an identity function per voice per sample)
// when distortion is off.
#if defined(DISTORTION_HARD_CLIP) || defined(DISTORTION_SOFT_CLIP) || \
    defined(DISTORTION_FOLDBACK) || defined(DISTORTION_BITCRUSH)
#define DISTORTION_ENABLED
#endif

// Where the distortion sits relative to the mix. Pick exactly ONE.
// PRE_MIX distorts each voice at full scale before it is scaled and summed,
// so DISTORTION_DRIVE_GAIN means the same thing at any voice count and
// voices don't intermodulate through one shared clipper. POST_MIX runs once
// on the summed output — cheaper, and the way this synth used to behave.
#define DISTORTION_STAGE_PRE_MIX
// #define DISTORTION_STAGE_POST_MIX

#if defined(DISTORTION_STAGE_PRE_MIX) == defined(DISTORTION_STAGE_POST_MIX)
#error "Define exactly one of DISTORTION_STAGE_PRE_MIX / DISTORTION_STAGE_POST_MIX"
#endif

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
