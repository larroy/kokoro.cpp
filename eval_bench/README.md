# eval_bench

G2P evaluation: compares kokoro.cpp's phonemizer against espeak-ng, reproducing how upstream
Kokoro (misaki `EspeakG2P`) built the voices' training data.

## Contents

- `g2p_dump.cpp` — CLI that reads lines on stdin and prints one phoneme string per line
  (`g2p_dump <language>`, currently only `es`).
- `compare_g2p.py` — runs `g2p_dump` and espeak-ng over the corpus, reports mismatches.
- `espeak_oracle.py` — espeak-ng phonemization matching misaki's backend setup and
  post-processing, without depending on misaki.
- `corpus/es.txt` — Spanish test corpus (comments with `#`, blank lines ignored).
- `corpus/es_known_diffs.tsv` — accepted mismatches, one per line: `<text>\t<comment>`.
  A line in this file with zero distance is reported as a *stale known diff* and should be
  removed.

## Running

```bash
uv run bootstrap.py build                       # builds build/eval_bench/g2p_dump
uv run --group eval python eval_bench/compare_g2p.py
```

Options: `--language es`, `--g2p-dump PATH`, `--report PATH` (TSV of every line with
`text, espeak, kokoro, distance, known` columns).

The script locates `g2p_dump` under `build/eval_bench/` (or the `Release` variant on
Windows); pass `--g2p-dump` to override.

espeak-ng is provided via Python packages (`espeakng_loader`, `phonemizer`), no system
install needed.

## Exit status

Exit code 1 when any mismatch is not listed in the language's known-diffs file — suitable
for CI. Output prints each mismatch with both phoneme strings, then a summary:
exact-match percentage and phoneme error rate (Levenshtein distance over code points).
