# Known issues

## 1. Chunk limit and truncation count UTF-8 bytes, not phonemes

**Where:**
- `split_phonemes`, `src/PhonemeChunker.cpp:20`: `current_batch.length() + part.length() + 1 >= max_length`.
- `Kokoro::_create_audio`, `src/Kokoro.cpp:218-219`: `phonemes.substr(0, MAX_PHONEME_LENGTH)`.

`MAX_PHONEME_LENGTH` (510, `src/Kokoro.h:20`) is meant to be a count of phonemes: the model takes at most 510 phoneme tokens plus two padding tokens, and a voice has one style row per length up to 510. Upstream Kokoro counts `len(ps)`, which is Python code points. Both places above use `std::string::length()` and `substr`, which count **bytes**.

**Consequences:**
- **Chunks are much shorter than they need to be.** Most IPA symbols take more than one byte in UTF-8 (`ʈ ʂ ɕ ŋ` take 2 bytes, tone arrows `→ ↗ ↘ ↓` take 3). For Chinese a 510-byte chunk holds far fewer than 510 phonemes, so text is split into more, shorter chunks than upstream would use. That changes prosody at chunk boundaries and selects style rows for shorter lengths.
- **Truncation can split a character.** `substr(0, 510)` can cut a multi-byte UTF-8 sequence in half. The tokenizer (`Tokenizer::tokenize` / `split_utf8`) then sees an invalid trailing fragment, which is not in the vocabulary and gets dropped. Truncation only happens when a single chunk is longer than 510 bytes, which is only possible for a segment that `split_phonemes` could not split (one with no `[.,!?;]` for 509+ bytes).
- **The two limits disagree.** The row index (fixed in `fbf9a6b`) uses the token count, while chunking uses bytes.

**Suggested fix:**
- Measure chunk length in phoneme tokens (or UTF-8 code points, matching upstream `len(ps)`) instead of bytes, both when packing in `split_phonemes` and when truncating in `_create_audio`.
- Truncate on a code-point boundary. It is simplest to tokenize first and cap the token vector at `MAX_PHONEME_LENGTH`.
- A segment longer than the limit should be split at word boundaries (spaces) before any hard truncation. That is what current kokoro-onnx does: its `split_phonemes` "prefer[s] splitting at punctuation marks, then at word boundaries" (`kokoro_onnx/chunker.py`).

**Testing notes:**
- `tests/test_chunker.cpp` can assert, for a long Chinese phoneme string:
  - every chunk has at most 510 code points or tokens;
  - chunks are not cut short at 510 bytes;
  - no chunk ends in a partial UTF-8 sequence.
- Expect the `g2p` goldens to be unaffected, because chunking happens after G2P. `synthesis` durations for long inputs will change.
