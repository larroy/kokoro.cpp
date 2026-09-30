# Known issues

## 1. `_split_phonemes` emits an empty chunk before a long first segment

**Where:** `Kokoro::_split_phonemes`, `src/Kokoro.cpp:161-163`.

`_split_phonemes` splits the phoneme string at `[.,!?;]` and packs the resulting segments into chunks. A new chunk starts when the current one would reach `MAX_PHONEME_LENGTH`:

```cpp
if (current_batch.length() + part.length() + 1 >= MAX_PHONEME_LENGTH) {
    batches.push_back(current_batch);   // pushed even when current_batch is empty
    current_batch = part;
}
```

If the first segment is 509 bytes or longer, `current_batch` is still empty when this branch runs, so an empty string is pushed as the first chunk. `Kokoro::create` then sends that chunk to `_create_audio`, which tokenizes it to nothing and runs the model on just the two padding tokens `[0, 0]` (with style row 0). Whatever audio that produces is prepended to the output. Only the first segment is affected: after it, `current_batch` always holds the previous segment, which is never empty because empty segments are skipped.

**What is known:**
- **Found by reading the code; not reproduced yet.** No run has shown the empty chunk directly. For example, the tests don't print the chunk list, and the empty chunk triggers no warning.
- **Not measured yet:** how much audio the empty `[0, 0]` chunk produces, and whether it is audible.
- **Only very long segments trigger it.** That means ≥ 509 bytes between punctuation marks, which real text rarely has, but `KOKORO_INPUT_PHONEMES` input or unpunctuated text can.

**Suggested fix:**
- Push `current_batch` only when it is non-empty, i.e. `if (!current_batch.empty()) batches.push_back(current_batch);`.
- Also consider skipping empty or token-less chunks in `create()`, as upstream does (`if not ps: continue` in `KPipeline.generate_from_tokens`), so no chunk with zero tokens ever reaches the model.

**Testing notes:**
- **No in-API oracle.** Output samples vary run to run (the vocoder adds noise), but output length is deterministic: `trim_audio` is a no-op and the duration predictor is deterministic. Still, no different input yields the same single long chunk without the empty chunk, so a C API test has nothing to compare the length against.
- **Test the chunker directly instead.** Move `_split_phonemes` out of `Kokoro` into a free function that needs no model, then unit-test the chunk list it returns. For example: no empty chunks, every chunk within the limit, concatenation preserves content.
- **Watch the link dependencies.** `kokoro_core` doesn't link ONNX Runtime (only `kokoro` does), so a test linking `kokoro_core` can only use the chunker if it lives outside `Kokoro.cpp`.

## 2. Chunk limit and truncation count UTF-8 bytes, not phonemes

**Where:**
- `Kokoro::_split_phonemes`, `src/Kokoro.cpp:161`: `current_batch.length() + part.length() + 1 >= MAX_PHONEME_LENGTH`.
- `Kokoro::_create_audio`, `src/Kokoro.cpp:185-186`: `phonemes.substr(0, MAX_PHONEME_LENGTH)`.

`MAX_PHONEME_LENGTH` (510, `src/Kokoro.h:18`) is meant to be a count of phonemes: the model takes at most 510 phoneme tokens plus two padding tokens, and a voice has one style row per length up to 510. Upstream Kokoro counts `len(ps)`, which is Python code points. Both places above use `std::string::length()` and `substr`, which count **bytes**.

**Consequences:**
- **Chunks are much shorter than they need to be.** Most IPA symbols take more than one byte in UTF-8 (`ʈ ʂ ɕ ŋ` take 2 bytes, tone arrows `→ ↗ ↘ ↓` take 3). For Chinese a 510-byte chunk holds far fewer than 510 phonemes, so text is split into more, shorter chunks than upstream would use. That changes prosody at chunk boundaries and selects style rows for shorter lengths.
- **Truncation can split a character.** `substr(0, 510)` can cut a multi-byte UTF-8 sequence in half. The tokenizer (`Tokenizer::tokenize` / `split_utf8`) then sees an invalid trailing fragment, which is not in the vocabulary and gets dropped. Truncation only happens when a single chunk is longer than 510 bytes, which is only possible for a segment that `_split_phonemes` could not split (issue 1's case).
- **The two limits disagree.** The row index (fixed in `fbf9a6b`) uses the token count, while chunking uses bytes.

**Suggested fix:**
- Measure chunk length in phoneme tokens (or UTF-8 code points, matching upstream `len(ps)`) instead of bytes, both when packing in `_split_phonemes` and when truncating in `_create_audio`.
- Truncate on a code-point boundary. It is simplest to tokenize first and cap the token vector at `MAX_PHONEME_LENGTH`.
- A segment longer than the limit should be split at word boundaries (spaces) before any hard truncation. That is what current kokoro-onnx does: its `split_phonemes` "prefer[s] splitting at punctuation marks, then at word boundaries" (`kokoro_onnx/chunker.py`).
- Fixing this together with issue 1 in one extracted, unit-tested chunker function is probably cleanest.

**Testing notes:**
- A unit test on the extracted chunker can assert, for a long Chinese phoneme string:
  - every chunk has at most 510 code points or tokens;
  - chunks are not cut short at 510 bytes;
  - no chunk ends in a partial UTF-8 sequence.
- Expect the `g2p` goldens to be unaffected, because chunking happens after G2P. `synthesis` durations for long inputs will change.
