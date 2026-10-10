# eval_bench

G2P evaluation: compares kokoro.cpp's phonemizer against espeak-ng, reproducing how upstream
Kokoro (misaki `EspeakG2P`) built the voices' training data.

## Contents

- `g2p_dump.cpp` — CLI that reads lines on stdin and prints one phoneme string per line
  (`g2p_dump <language> [dict_dir]`, language `es` or `de`; `dict_dir` defaults to `dict`).
- `compare_g2p.py` — runs `g2p_dump` and espeak-ng over the corpus, reports mismatches.
- `espeak_oracle.py` — espeak-ng phonemization matching misaki's backend setup and
  post-processing, without depending on misaki.
- `corpus/es.txt`, `corpus/de.txt` — Spanish and German test corpora (comments with `#`, blank lines ignored).
- `corpus/es_known_diffs.tsv`, `corpus/de_known_diffs.tsv` — accepted mismatches, one per line: `<text>\t<comment>`.
  A line in this file with zero distance is reported as a *stale known diff* and should be
  removed.

## Running

```bash
uv run bootstrap.py build                       # builds build/eval_bench/g2p_dump
uv run --group eval python eval_bench/compare_g2p.py
```

Options: `--language es|de`, `--g2p-dump PATH`, `--report PATH` (TSV of every line with
`text, espeak, kokoro, distance, known` columns).

The script locates `g2p_dump` under `build/eval_bench/` (or the `Release` variant on
Windows); pass `--g2p-dump` to override.

espeak-ng is provided via Python packages (`espeakng_loader`, `phonemizer`), no system
install needed.

Secondary stress (`ˌ`) is removed from espeak-ng's output before comparing, as kokoro.cpp never writes it. For
German, espeak-ng's `ʏ` is also compared as `y`: Kokoro's vocab has no `ʏ`, so the German G2P writes `y`.

## Exit status

Exit code 1 when any mismatch is not listed in the language's known-diffs file — suitable
for CI. Output prints each mismatch with both phoneme strings, then a summary:
exact-match percentage and phoneme error rate (Levenshtein distance over code points).
