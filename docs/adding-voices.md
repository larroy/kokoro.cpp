# Adding voices

Voice style packs live in the voices file (`models/voices-v1.1-zh.bin` by default). To add a compatible voice,
write a new voices file that contains it and point kokoro at that file. No rebuild is needed. The model weights
still determine which speech characteristics a voice pack can produce; fine-tuning those weights also requires
a new model export.

There are three ways to get a new voice:

1. [Blend existing voices](#1-blend-existing-voices). Needs only [uv](https://docs.astral.sh/uv/).
2. [Import a voice tensor](#2-import-a-voice-tensor-pt) (`.pt`), for example one downloaded from Hugging Face or produced by a voice-cloning tool. Needs PyTorch, installed by uv's `voice` dependency group.
3. [Make a voice from your own recordings](#3-a-voice-from-your-own-recordings). No official tool exists for this; see the notes below.

For Spanish, see [Spanish voices](#spanish-voices) for commands and
[Training a new Spanish voice](#training-a-new-spanish-voice) for the experimental training workflow.

## What a voice is

A voice is a table of **510 rows × 256 float32 values** (130,560 floats).

- Each row is a style vector for one input length. kokoro.cpp picks row *N* − 1 for a chunk of *N* phoneme tokens (`Kokoro::_create_audio` in `src/Kokoro.cpp`), as upstream Kokoro does, and long text is split into chunks of at most 510 tokens.
- Within a row, the first 128 values feed the decoder (timbre) and the last 128 feed the prosody predictor (duration, pitch, energy).
- Keep voice packs paired with the model weights they were made for. The default pack accompanies
  [Kokoro-82M-v1.1-zh](https://huggingface.co/hexgrad/Kokoro-82M-v1.1-zh) in `models/kokoro-v1.1-zh.onnx`.
  The Spanish pack, `models/voices-v1.0-es.bin`, accompanies `models/kokoro-v1.0.onnx`.

## Voices file format

All integers are little-endian `uint32`.

| Field | Contents |
|---|---|
| magic | the 4 bytes `VOIC` |
| version | `1` |
| count | number of voices |

This is followed by `count` entries:

| Field | Contents |
|---|---|
| name length | length of the name in bytes |
| name | UTF-8, no terminating NUL |
| float count | `130560` (510 × 256) |
| data | `float count` little-endian float32 values, row by row |

How the loader (`Kokoro::load_voices`) treats the file:
- Names are case-sensitive.
- If a name appears twice, the later entry wins.
- `--list-voices` and `kokoro_voice_name()` return the names sorted.
- Voices must be a whole number of 256-float rows, or loading fails.
- Always write full 510 × 256 tables. With fewer rows, longer chunks all reuse the last row, which was made for a different length.

## `voice_tool.py`

`voice_tool.py` in the repository root reads a voices file, adds one voice to it, and writes the result. Every command takes:
- `--voices <path>`: the voices file to start from (default: `models/voices-v1.1-zh.bin`)
- `--name <name>`: name of the new voice; a voice with the same name is replaced
- `-o, --output <path>`: the voices file to write (required)

The output file keeps every voice from the input and adds the new one, so it can replace the default file. Don't overwrite the downloaded `voices-v1.1-zh.bin`; keep it as the source.

To add several voices, pass the previous output back as `--voices`:

```bash
uv run voice_tool.py blend --name af_maplevale -o models/voices-custom.bin af_maple:0.7 bf_vale:0.3
uv run --group voice voice_tool.py import-pt --voices models/voices-custom.bin --name af_myvoice -o models/voices-custom.bin my_voice.pt
```

The repository is a uv project (`pyproject.toml`): `uv run` installs the base dependencies (click) into `.venv` on first use. The heavy dependencies, PyTorch (CPU build) and NumPy, are in the `voice` dependency group; install them with `uv sync --group voice`, or pass `--group voice` to `uv run`.

## 1. Blend existing voices

A weighted average of two or more voices gives a new voice somewhere between them. This is the same operation as upstream Kokoro's `KPipeline.load_voice("a,b")`, which takes the plain mean. Run from the repository root:

```bash
uv run voice_tool.py blend --name af_maplevale -o models/voices-custom.bin af_maple:0.7 bf_vale:0.3
```

Each argument is `<voice>[:<weight>]`. The weight defaults to 1 and must be positive. Weights are normalized to sum to 1, so `af_maple bf_vale` is the plain mean.

Voices can be blended across languages (for example `zf_002` with `af_maple`), but the result is less predictable. Listen to it before relying on it.

## 2. Import a voice tensor (`.pt`)

Upstream Kokoro and tools built on it store each voice as a PyTorch tensor of shape `(510, 1, 256)`, saved as `<name>.pt`. Importing one needs PyTorch from the `voice` group:

```bash
uv run --group voice voice_tool.py import-pt --name af_myvoice -o models/voices-custom.bin my_voice.pt
```

The tensor must hold 510 × 256 values; anything else is rejected.

Where `.pt` voices come from:
- **[Kokoro-82M-v1.1-zh `voices/`](https://huggingface.co/hexgrad/Kokoro-82M-v1.1-zh/tree/main/voices).** These are the voices made for this model, and all 103 are already in the bundled voices file.
- **Other Kokoro releases,** such as the v1.0 voices (`af_heart`, `am_adam`, …) in [hexgrad/Kokoro-82M](https://huggingface.co/hexgrad/Kokoro-82M/tree/main/voices). They have the same shape and load without errors, but they were made for different model weights. Expect the result to sound different from the upstream samples, and listen before relying on them.

If you have a whole voice set as a NumPy file (a `.npy` dict or a `.npz` mapping names to arrays), `uv run --group voice scripts/export_voices.py <voices.npy> <output.bin>` converts all of it in one go. It needs NumPy (in the `voice` group), and it writes only the voices in its input, so include the bundled ones if you want to keep them.

## 3. A voice from your own recordings

Kokoro's official release omits the audio-to-style encoder, so it provides no official workflow to compute a
voice pack from a recording. See the [model card](https://huggingface.co/hexgrad/Kokoro-82M#model-facts).
Community tools provide experimental alternatives; the following describes their published capabilities as of
October 2026, not results validated by this repository:

- [KVoiceWalk](https://github.com/RobViren/kvoicewalk) searches for a style tensor whose generated speech resembles
  a reference recording, using a random walk scored by speaker similarity and audio features. It writes a `.pt`
  pack. Its published pipeline uses English and Kokoro v1.0; Spanish requires adapting the language configuration
  and evaluation texts. Keep the search model identical to the deployment model. GPU acceleration is useful,
  and results vary between runs.
- [Inno's voice tuner](https://huggingface.co/remsky/kokoro-inno-clone-tuner) generates a compatible
  `(510, 1, 256)` pack from a short reference recording. Its published training is English-based, so Spanish
  quality needs evaluation. Treat it as an approximation of a speaker, not a guaranteed faithful clone.
- [GushiLabs' encoder project](https://github.com/gushilabs/train-kokoro-encoder-styletts2) reconstructs missing
  encoders and produces Kokoro v1.0-compatible packs. Its authors describe cloning quality as experimental.

Import compatible output with [option 2](#2-import-a-voice-tensor-pt). Generating or optimizing a pack while
keeping Kokoro frozen leaves the ONNX model unchanged. Training the model itself is a separate route, described
below.

## Spanish voices

`uv run bootstrap.py configure` installs Kokoro v1.0 and `models/voices-v1.0-es.bin`, containing all three
[official Spanish voices](https://huggingface.co/hexgrad/Kokoro-82M/blob/main/VOICES.md#spanish):
`ef_dora`, `em_alex`, and `em_santa`.

| Approach | Result | Deployment artifacts |
|---|---|---|
| Blend existing voices | Variations in timbre and delivery | New voices file, existing model |
| Generate or optimize a style pack | Approximation of a reference speaker | New voices file, existing model |
| Fine-tune model weights | More capacity to learn a speaker's characteristics | New model and matching voice pack |

Blending is available now. Run these commands from the repository root (Bash syntax):

```bash
uv run voice_tool.py blend \
  --voices models/voices-v1.0-es.bin \
  --name em_custom -o models/voices-custom-es.bin \
  em_alex:0.7 em_santa:0.3

./build/kokoro -m models/kokoro-v1.0.onnx \
  --voices models/voices-custom-es.bin \
  -v em_custom "Hola, esta es una nueva voz."
```

In PowerShell, use backticks instead of backslashes for line continuation and
`.\build\Release\kokoro.exe` for the executable in a standard Windows Release build.

A blend interpolates existing style packs; it does not learn a new speaker or dialect. To import a compatible
pack produced from recordings instead:

```bash
uv run --group voice voice_tool.py import-pt \
  --voices models/voices-v1.0-es.bin \
  --name em_custom -o models/voices-custom-es.bin em_custom.pt
```

Keep the original Spanish pack as the source. To add further voices to the custom pack, pass
`--voices models/voices-custom-es.bin`. Names beginning with `ef_` or `em_` select Spanish phonemization
automatically; use `--language es` to force it for another name. Always explicitly select the v1.0 model for
these voices, since the CLI defaults to v1.1-zh.

## Training a new Spanish voice

This repository provides inference and voice packaging, not a training pipeline. A practical experimental
workflow is:

1. **Choose a speaker and dialect.** Gather clean recordings with accurate transcripts and permission to use
   them. Budget roughly **1-3 hours per speaker** for an initial fine-tuning pilot; this is a planning estimate,
   not a demonstrated minimum or quality guarantee. Short-reference pack generators have different requirements.
2. **Prepare the dataset.** Convert audio to 24 kHz mono, segment into short utterances, correct transcripts,
   and remove clipping and overlapping speakers. Reserve recordings and sentences for held-out evaluation.
3. **Match pronunciation and alignment.** Convert transcripts to Kokoro-compatible phonemes and align them with
   the audio. Training and C++ inference must agree on pronunciation rules and phoneme token IDs.
4. **Try embedding-only training first.** Freeze Kokoro's weights and optimize a speaker style representation.
   Export a complete `(510, 1, 256)` pack compatible with the runtime's length-based row selection, and evaluate
   it across short and long inputs. A successful result needs only a new voices file.
5. **Fine-tune model components if needed.** If the frozen model cannot reproduce the speaker adequately, train
   selected prosody and decoder components. The community
   [kokoro-finetune project](https://github.com/hidude562/kokoro-finetune) provides a starting point with alignment,
   embedding training, and model fine-tuning. Its published examples are English; Spanish preprocessing,
   alignment, and synthesis need adaptation and validation. This is not a turnkey Spanish recipe.
6. **Evaluate and export.** Listen to unseen Spanish sentences for intelligibility, speaker similarity,
   pronunciation, pacing, and artifacts. Include numbers, questions, short phrases, and long passages; check
   transcription errors as well as speaker similarity. If weights changed, export a new ONNX model with the
   token/style/speed interface expected by kokoro.cpp and deploy its matching voice pack. Compare Python and
   C++ synthesis, and check existing voices for regressions if they will share the new model.

Use a separate `uv` project/environment with GPU-enabled PyTorch for training. This repository's `voice`
dependency group selects CPU PyTorch on non-macOS platforms and is intended for importing and packaging voices.
Training time and GPU memory needs depend on the recipe, clip length, and which model components are trainable.

### Spanish dialects

The current C++ phonemizer implements Castilian pronunciation conventions, including `θ` for appropriate
`c`/`z` spellings and `ʎ` for `ll`. A Mexican, Colombian, or other dialect target also needs matching
pronunciation rules and evaluation: changing the voice embedding alone does not reliably supply the accent.
See `src/spanish/SpanishG2P.cpp` and the existing G2P benchmark in `eval_bench/README.md`.

Start with one target speaker and a frozen-model pack experiment evaluated on Spanish sentences. This tests
whether useful additional voices are possible with the existing runtime before investing in full fine-tuning.

## Using the new voice

Pass the new voices file with `--voices`:

```bash
./build/kokoro --voices models/voices-custom.bin --list-voices
./build/kokoro --voices models/voices-custom.bin --voice af_maplevale "Hello world"
```

From the C API, pass it as `voices_path`:

```c
kokoro_create("models/kokoro-v1.1-zh.onnx", "models/voices-custom.bin", "dict", &ctx);
```

## Naming

Follow the existing `<language><gender>_<name>` convention:

- The language letter is `a` (American English), `b` (British English), `e` (Spanish), or `z` (Mandarin Chinese).
- The gender letter is `f` or `m`.

In automatic language mode, `ef_`/`em_` names select Spanish G2P, `af_`/`am_`/`bf_`/`bm_` select English,
and other names use Chinese/English phonemization by script. `--language es|en|zh` or the C API's
`kokoro_set_language()` overrides this selection. `kokoro_phonemize()` has no voice argument, so select Spanish
explicitly when using it. Naming a voice does not change the model weights or train an accent.
