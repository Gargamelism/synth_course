#pragma once

#include <math.h>

// Harmonic spread — the shape of a voice's overtone series. A runtime value,
// not a build switch: each spread is baked into its own wavetable at startup,
// so a voice's spread is just which table it reads, and voices are free to
// use different ones (see voices.h for the per-voice assignment).
//
// harmonicWeightTerm() at the bottom is the single entry point: the weight of
// harmonic h (h = 0 is the fundamental, n = h + 1 its multiple) for a spread.
// Only ever called while oscillator.cpp builds a wavetable at startup, so
// nothing here touches the audio path — a spread may be as slow or as long a
// table as it likes. buildTable() renormalizes the weights to sum to one, so
// only their ratios matter; tables below are linear amplitudes relative to
// the loudest harmonic.
//
// Everything is header-only (inline) so oscillator.cpp is still the only
// translation unit the host tests need to link.
enum HarmonicSpread {
  SPREAD_SINE = 0, // a single sine wave, no overtones
  SPREAD_SAW,       // 1/n falloff — a real string/wind instrument
  SPREAD_OCTAVE,      // octave harmonics only — hollow, organ-like
  SPREAD_SQUARE,         // odd harmonics only — square/clarinet-like
  SPREAD_EQUAL,       // no falloff — dense and buzzy
  SPREAD_TRIANGLE,    // 1/n^2 falloff — a triangle wave, mellow and bowed
  SPREAD_PULSATING,      // 
  SPREAD_VIOLA,       // 1/n plus a 3rd-6th formant bump, mellow top end
  SPREAD_OCTAVES_WITH_ODDS,
  // Instrument models — see each weight function for the data and source.
  SPREAD_GUITAR,          // plucked string, plucked at 1/5 of its length
  SPREAD_PIANO,           // struck string, hammer at 1/8 of its length
  SPREAD_CLARINET,        // chalumeau register: odd strong, even weak
  SPREAD_FLUTE,           // ~7 harmonics, fast monotone rolloff
  SPREAD_TRUMPET,         // brass formant: harmonics 2-5 peak
  SPREAD_HAMMOND_TIBIA,   // Hammond registration 00 8040 000
  SPREAD_HAMMOND_TRUMPET, // Hammond registration 00 7888 872
  SPREAD_COUNT
};

// --- Synthetic spreads -----------------------------------------------------

inline float weightSine(int h) {
  return h == 0 ? 1.0f : 0.0f;
}

// The nth harmonic is 1/n as loud as the fundamental — a real string/wind
// instrument's overtone falloff.
inline float weightSaw(int h) {
  return 1.0f / (h + 1);
}

// Only octave harmonics (1x, 2x, 4x, 8x, 16x...) sound, each at the natural
// series' 1/n falloff; every non-octave harmonic is silent. Sparse and
// hollow, like an organ's octave-only stops.
inline bool isPowerOfTwo(int n) {
  return n > 0 && (n & (n - 1)) == 0;
}
inline float weightOctave(int h) {
  const int n = h + 1;
  return isPowerOfTwo(n) ? 1.0f / n : 0.0f;
}

// Only odd harmonics (1x, 3x, 5x...) sound, at the natural series' 1/n
// falloff; even harmonics are silent — a square/clarinet-like spectrum.
inline float weightSquare(int h) {
  const int n = h + 1;
  return (n % 2 == 1) ? 1.0f / n : 0.0f;
}

inline float weightTriangle(int h) {
  const int n = h + 1;
  return !isPowerOfTwo(n) ? 1.0f / (n * n) : 0.0f;
}

inline float weightPulsating(int h) {
  const int n = h + 1;
  const float width = 0.5f; // 50% duty cycle
  return fabsf(sinf((float)M_PI * n * width)) / n;
}

// Every harmonic at equal weight, no falloff — dense and buzzy.
inline float weightEqual(int h) {
  (void)h;
  return 1.0f;
}

// Cheap bowed-string approximation: natural 1/n falloff, plus a boosted
// "formant" bump on the 3rd-6th harmonics (the nasal, woody quality that
// separates a viola from a plain sawtooth spectrum), plus a faster rolloff
// above the 8th harmonic for a mellower top end. The bump sits on fixed
// harmonic numbers rather than a fixed frequency, so — unlike a real
// viola's body resonance — it shifts with pitch instead of staying put;
// still close enough to be recognizable across this synth's pitch range.
inline float weightViola(int h) {
  const int n = h + 1;
  const float falloff = 1.0f / n;
  const float formant = (n >= 3 && n <= 6) ? 1.6f : 1.0f;
  const float highRolloff = (n > 8) ? 0.5f : 1.0f;
  return falloff * formant * highRolloff;
}

inline float weightOctavesWithSomeOdds(int h) {
  const int n = h + 1;
  if (isPowerOfTwo(n)) return 1.0f / n; // octave harmonics
  return 0.5f / n; // odd harmonics
}

// --- Instrument models -----------------------------------------------------
// Like the viola above, every model pins its shape to harmonic NUMBERS, so a
// real instrument's fixed-frequency formants and body resonances shift with
// pitch here. Each is tuned to be recognizable around the middle of this
// synth's range.

// Harmonics past the end of a table are silent.
inline float weightFromTable(const float *table, int len, int h) {
  return h < len ? table[h] : 0.0f;
}

// Ideal plucked string, plucked at fraction p of its length: the nth
// harmonic's amplitude is sin(n*pi*p) / n^2, so harmonics with a node at the
// pluck point (multiples of 1/p) vanish and the rest fall off at 1/n^2 —
// warmer than a bowed string's 1/n. p = 1/5 is a normal guitar picking
// position, killing the 5th, 10th, 15th and 20th harmonics.
// Source: D. Russell, "Fourier series of a plucked string",
//   https://www.acs.psu.edu/drussell/Demos/Pluck-Fourier/Pluck-Fourier.html
//   (A_n = 2h/(n^2 pi^2) * L^2/(d(L-d)) * sin(d n pi / L));
//   Fletcher & Rossing, The Physics of Musical Instruments, 2nd ed., ch. 2.
inline float weightGuitar(int h) {
  const int n = h + 1;
  const float pluckPosition = 0.2f;
  return fabsf(sinf((float)M_PI * n * pluckPosition)) / (float)(n * n);
}

// Struck string, hammer at fraction p of its length. A hammer imparts
// velocity rather than displacement, so the falloff is 1/n (not 1/n^2),
// still with sin(n*pi*p) killing the harmonics with a node at the strike
// point. Piano hammers sit at p ~ 1/7..1/8 precisely so the dissonant 7th
// and 8th harmonics are weak; the extra 1/(1 + (n/8)^2) term stands in for
// the hammer's finite contact width/time, which low-passes the spectrum.
// Source: Fletcher & Rossing, The Physics of Musical Instruments, 2nd ed.,
//   ch. 2 (struck string) and ch. 12 (the piano hammer position).
inline float weightPiano(int h) {
  const int n = h + 1;
  const float strikePosition = 1.0f / 8.0f;
  const float hammerLowpass = 1.0f / (1.0f + (n / 8.0f) * (n / 8.0f));
  return fabsf(sinf((float)M_PI * n * strikePosition)) / (float)n * hammerLowpass;
}

// Clarinet, chalumeau (low) register. The bore is a closed cylinder, so only
// the odd harmonics line up with its resonances: the fundamental and 3rd are
// strong, the 2nd is ~25 dB down, and the even/odd gap narrows with
// harmonic number until it vanishes around the 9th, where the tone is just a
// 1/n-ish rolloff. Values are dB-to-linear readings of the UNSW spectra.
// Source: J. Wolfe, "Clarinet acoustics: an introduction",
//   https://www.phys.unsw.edu.au/jw/clarinetacoustics.html and the per-note
//   spectra, e.g. https://newt.phys.unsw.edu.au/music/clarinet/C4.html
inline float weightClarinet(int h) {
  static constexpr float kTable[] = {
      // n:  1     2     3     4     5     6     7     8
      1.00f, 0.06f, 0.35f, 0.05f, 0.22f, 0.06f, 0.16f, 0.07f,
      // n:  9    10    11    12    13    14    15    16
      0.12f, 0.09f, 0.09f, 0.08f, 0.07f, 0.06f, 0.06f, 0.05f,
      // n: 17    18    19    20    21    22
      0.05f, 0.04f, 0.04f, 0.03f, 0.03f, 0.03f};
  return weightFromTable(kTable, sizeof(kTable) / sizeof(kTable[0]), h);
}

// Flute: nearly a sine. Only the first seven or so harmonics are audible,
// each roughly half the previous one, and there is nothing above the 8th.
// Louder playing brings the upper ones up; this table is a mezzo-forte.
// Source: Physics Classroom, "The Sound of Music" (flute: ~7 harmonics whose
//   intensity decreases monotonically),
//   https://www.physicsclassroom.com/getattachment/actprep/act10ag.pdf ;
//   J. Wolfe, "Flute acoustics", https://www.phys.unsw.edu.au/jw/fluteacoustics.html
inline float weightFlute(int h) {
  static constexpr float kTable[] = {
      // n:  1     2     3     4     5     6     7     8
      1.00f, 0.50f, 0.20f, 0.10f, 0.05f, 0.03f, 0.02f, 0.01f};
  return weightFromTable(kTable, sizeof(kTable) / sizeof(kTable[0]), h);
}

// Trumpet (mid range, forte). Brass radiates most strongly at a few hundred
// Hz to ~1.3 kHz — a "formant" on the 2nd-5th harmonics for a note around
// 300 Hz — with the fundamental ~3 dB below the peak and a gradual rolloff
// above, the bell acting as a megaphone rather than a resonator up there.
// The shape matches Hammond's own "French Trumpet" registration (7888 872:
// harmonics 2-5 full, fundamental one step down).
// Source: J. Wolfe, "Brass instrument acoustics",
//   https://www.phys.unsw.edu.au/jw/brassacoustics.html ; HyperPhysics,
//   trumpet formant 1200-1400 Hz,
//   https://hyperphysics.gsu.edu/hbase/Music/orchins.html
inline float weightTrumpet(int h) {
  static constexpr float kTable[] = {
      // n:  1     2     3     4     5     6     7     8
      0.70f, 1.00f, 1.00f, 1.00f, 0.90f, 0.70f, 0.55f, 0.45f,
      // n:  9    10    11    12    13    14    15    16
      0.35f, 0.28f, 0.22f, 0.18f, 0.15f, 0.12f, 0.10f, 0.08f,
      // n: 17    18    19    20    21    22
      0.07f, 0.06f, 0.05f, 0.04f, 0.035f, 0.03f};
  return weightFromTable(kTable, sizeof(kTable) / sizeof(kTable[0]), h);
}

// Hammond tonewheel organ registrations. `drawbars` is the seven digits for
// the 8' 4' 2-2/3' 2' 1-3/5' 1-1/3' 1' drawbars, i.e. harmonics 1, 2, 3, 4,
// 5, 6 and 8 (there is no 7th-harmonic drawbar; the 16' and 5-1/3' sub-
// drawbars sit below the fundamental and are left out). Each position is
// nominally 3 dB louder than the last; the table uses measured levels.
// Sources: HammondWiki, "Drawbars", https://www.dairiki.org/HammondWiki/Drawbars ;
//   S. Vorkoetter, "The Science of Hammond Organ Drawbar Registration"
//   (measured per-position dB, classic registrations),
//   https://www.stefanv.com/electronics/hammond_drawbar_science.html ;
//   Hammond, "Drawbars & Percussion",
//   https://hammondorganco.com/wp-content/uploads/2015/06/03-DRAWBARS-PERCUSSION-corrected.pdf
inline float weightHammond(const char *drawbars, int h) {
  static constexpr int kHarmonicOfDrawbar[7] = {1, 2, 3, 4, 5, 6, 8};
  static constexpr float kLevelDb[9] = {-1000.0f, -32.1f, -26.1f, -20.0f, -15.2f,
                                        -10.5f, -6.0f, -3.0f, 0.0f};
  const int n = h + 1;
  for (int i = 0; i < 7; i++) {
    if (kHarmonicOfDrawbar[i] != n) continue;
    const int position = drawbars[i] - '0';
    if (position <= 0) return 0.0f;
    return powf(10.0f, kLevelDb[position] / 20.0f);
  }
  return 0.0f;
}

// "Tibia 8'": fundamental plus a 3rd harmonic 15 dB down — a soft, flute-like
// theatre-organ stop.
inline float weightHammondTibia(int h) {
  return weightHammond("8040000", h);
}

// "French Trumpet 8'": harmonics 2-5 full out, fundamental and 6th one step
// down, a touch of 8th — Hammond's brassy registration.
inline float weightHammondTrumpet(int h) {
  return weightHammond("7888872", h);
}

// --- Dispatch --------------------------------------------------------------

inline float harmonicWeightTerm(int spread, int h) {
  switch (spread) {
    case SPREAD_SINE:              return weightSine(h);
    case SPREAD_OCTAVE:            return weightOctave(h);
    case SPREAD_SQUARE:            return weightSquare(h);
    case SPREAD_EQUAL:             return weightEqual(h);
    case SPREAD_VIOLA:             return weightViola(h);
    case SPREAD_OCTAVES_WITH_ODDS: return weightOctavesWithSomeOdds(h);
    case SPREAD_GUITAR:            return weightGuitar(h);
    case SPREAD_PIANO:             return weightPiano(h);
    case SPREAD_CLARINET:          return weightClarinet(h);
    case SPREAD_FLUTE:             return weightFlute(h);
    case SPREAD_TRUMPET:           return weightTrumpet(h);
    case SPREAD_HAMMOND_TIBIA:     return weightHammondTibia(h);
    case SPREAD_HAMMOND_TRUMPET:   return weightHammondTrumpet(h);
    case SPREAD_PULSATING:         return weightPulsating(h);
    case SPREAD_TRIANGLE:          return weightTriangle(h);
    case SPREAD_SAW:
    default:                       return weightSaw(h);
  }
}
