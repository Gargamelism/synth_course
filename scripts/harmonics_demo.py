"""
Python mirror of oscillator.cpp's harmonicWeightTerm()/initHarmonicWeights(),
for exploring the HARMONIC_SPREAD_* options without recompiling firmware.
"""

NUM_HARMONICS = 22          # matches pins.h
VOLUME_Q15_ONE = 32767      # matches pins.h ((1 << 15) - 1)


def harmonic_weight_term(h, spread):
    n = h + 1  # harmonic number: h=0 -> fundamental (1x), h=1 -> 2x, ...

    if spread == "natural":
        # 1/n falloff — every harmonic present, quieter as they go up.
        return 1.0 / n

    if spread == "octave":
        # Only 1x, 2x, 4x, 8x, 16x... survive; everything else is silent.
        is_power_of_two = n > 0 and (n & (n - 1)) == 0
        return 1.0 / n if is_power_of_two else 0.0

    if spread == "odd":
        # Only odd harmonics (1x, 3x, 5x...) survive, still 1/n falloff.
        return 1.0 / n if n % 2 == 1 else 0.0

    if spread == "equal":
        # No falloff at all — every harmonic exactly as loud as the rest.
        return 1.0

    raise ValueError(spread)


def compute_weights(spread, num_harmonics=NUM_HARMONICS):
    # Pass 1: raw terms, before normalization.
    terms = [harmonic_weight_term(h, spread) for h in range(num_harmonics)]

    # Pass 2: normalize so they sum to 1.0 — this is what keeps the
    # oscillator's worst-case (all-harmonics-in-phase) peak from exceeding
    # SINE_TABLE_AMPLITUDE, same bound a single un-enriched sine has.
    total = sum(terms)
    weights = [t / total for t in terms]

    # Pass 3: quantize to Q15 fixed point, same rounding as the C++.
    q15 = [int(w * VOLUME_Q15_ONE + 0.5) for w in weights]

    return terms, weights, q15


def show(spread):
    terms, weights, q15 = compute_weights(spread)
    print(f"\n=== HARMONIC_SPREAD_{spread.upper()} ===")
    print(f"{'n':>3} {'raw term':>10} {'weight':>10} {'Q15':>7}")
    for h in range(NUM_HARMONICS):
        n = h + 1
        if q15[h] == 0 and spread != "natural":
            continue  # skip silent harmonics for the sparse spreads
        print(f"{n:>3} {terms[h]:>10.4f} {weights[h]:>10.4f} {q15[h]:>7}")
    print(f"sum(Q15) = {sum(q15)}  (target VOLUME_Q15_ONE = {VOLUME_Q15_ONE})")


if __name__ == "__main__":
    for spread in ("natural", "octave", "odd", "equal"):
        show(spread)
