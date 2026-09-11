#pragma once

#include <stdint.h>

// Distortion applied in audio_task.cpp, either per voice or to the mixed
// sample (see DISTORTION_STAGE_* below). Which flavor plays is a per-patch,
// runtime choice now (see DistortionType and Patch::distortion in
// voices.h) — every flavor below is always compiled in so the encoder can
// switch to any of them without a reflash.

enum DistortionType {
  DIST_NONE = 0,
  DIST_HARD_CLIP,
  DIST_SOFT_CLIP,
  DIST_FOLDBACK,
  DIST_BITCRUSH,
};

// Where the distortion sits relative to the mix. Pick exactly ONE.
// PRE_MIX distorts each voice at full scale before it is scaled and summed,
// so DISTORTION_DRIVE_GAIN means the same thing at any voice count and
// voices don't intermodulate through one shared clipper. POST_MIX runs once
// on the summed output — cheaper, and the way this synth used to behave.
// This stays a build-time choice: unlike the flavor, it isn't part of what
// the encoder switches.
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

// Applies `type` to one sample (DIST_NONE is a passthrough). The caller
// (audio_task.cpp) skips this call entirely when the active patch's
// distortion is toggled off, rather than routing every sample through a
// DIST_NONE switch case.
int16_t applyDistortion(int16_t sample, DistortionType type);
