# Adding voices

Voices live in the voices file (`models/voices-v1.1-zh.bin` by default), not in the model. To add a voice, write a new voices file that contains it and point kokoro at that file. No rebuild is needed.

There are three ways to get a new voice:

1. [Blend existing voices](#1-blend-existing-voices). Needs only [uv](https://docs.astral.sh/uv/).
2. [Import a voice tensor](#2-import-a-voice-tensor-pt) (`.pt`), for example one downloaded from Hugging Face or produced by a voice-cloning tool. Needs PyTorch, installed by uv's `voice` dependency group.
3. [Make a voice from your own recordings](#3-a-voice-from-your-own-recordings). No official tool exists for this; see the notes below.

## What a voice is

A voice is a table of **510 rows × 256 float32 values** (130,560 floats).

- Each row is a style vector for one input length. kokoro.cpp picks row *N* − 1 for a chunk of *N* phoneme tokens (`Kokoro::_create_audio` in `src/Kokoro.cpp`), as upstream Kokoro does, and long text is split into chunks of at most 510 tokens.
- Within a row, the first 128 values feed the decoder (timbre) and the last 128 feed the prosody predictor (duration, pitch, energy).
- A voice only works well with the model it was made for. The bundled voices were made for [Kokoro-82M-v1.1-zh](https://huggingface.co/hexgrad/Kokoro-82M-v1.1-zh), the model in `models/kokoro-v1.1-zh.onnx`.

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

This can't be done directly. Kokoro's release is "decoder only: no diffusion, no encoder release" ([model card](https://huggingface.co/hexgrad/Kokoro-82M-v1.1-zh)), so there is no style encoder to compute a voice from audio.

What works instead is searching for a tensor whose output sounds like a target recording. [KVoiceWalk](https://github.com/RobViren/kvoicewalk) does this with a random walk scored by speaker similarity, and it writes a `.pt` file that you import with [option 2](#2-import-a-voice-tensor-pt). Caveats:
- As published, KVoiceWalk runs the v1.0 model (`KPipeline(lang_code="a", repo_id="hexgrad/Kokoro-82M")` in `utilities/speech_generator.py`). A tensor it finds is tuned to v1.0 weights. To tune it for this repository's model, point that line at `hexgrad/Kokoro-82M-v1.1-zh`.
- The search needs a GPU to finish in reasonable time, and its results vary from run to run.

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
- The language letter is `a` (American English), `b` (British English) or `z` (Mandarin Chinese).
- The gender letter is `f` or `m`.

This is only a convention. The name doesn't pick the G2P or the language: text is always phonemized by script, whatever the voice.
