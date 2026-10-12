# LESS Side-Channel Analysis — Reports

Two reports I wrote analysing possible cache side-channel vulnerabilities in **LESS**
(the Linear Equivalence Signature Scheme, a post-quantum digital signature candidate).
These are write-ups and proof-of-concept exploration — the LESS reference implementation
itself is **not** fully reproduced here, so treat this repo as the reports plus the
supporting bits I used while poking at it.

## Reports

- **[`reports/LESS_Report1_FlushReload.pdf`](reports/LESS_Report1_FlushReload.pdf)** —
  *Side-Channel Analysis of LESS.* Looks at `prepare_digest_input()` and the
  `lex_minimize()` routine, and how a Flush+Reload attack on that code region could leak
  the `is_pivot_column` vector, and what that does (and doesn't) tell you about the secret
  monomial matrix.

- **[`reports/LESS_Report2_PrimeProbe.pdf`](reports/LESS_Report2_PrimeProbe.pdf)** —
  *Detailed Analysis of Prime+Probe Vulnerabilities in LESS.* Looks at the
  `yt_shuffle_state` Fisher–Yates permutation shuffle, how a Prime+Probe attack could
  target its secret-dependent swaps, a Mastik-based simulation, and what it would take to
  reconstruct the permutation part of the key. Includes the results (and the parts that
  didn't work) honestly.

Both reports conclude that the leakage reduces the key search space but, on its own,
isn't enough for full key recovery under realistic parameters.

## What else is in here

- `Mastik/` — the [Mastik](https://cs.adelaide.edu.au/~yval/Mastik/) micro-architectural
  side-channel toolkit, used for the cache probing experiments, plus the small
  `attacker.c` / `multi_attack.c` I was experimenting with.
- `less-submission-round2/` — the LESS Round 2 NIST submission package, kept as reference
  for the functions the reports analyse.

## Note

This is coursework / research exploration into post-quantum crypto side channels. It's
for understanding and defending against these leaks, not for attacking anyone's systems.

— Shreyash Kumar
