# Adding voices

Voices live in the voices file (`models/voices-v1.1-zh.bin` by default), not in the model. To add a voice, write a new voices file that contains it and point kokoro at that file. No rebuild is needed.

There are three ways to get a new voice:

1. [Blend existing voices](#1-blend-existing-voices). Needs only Python 3.
2. [Import a voice tensor](#2-import-a-voice-tensor-pt) (`.pt`), for example one downloaded from Hugging Face or produced by a voice-cloning tool. Needs PyTorch.
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

## Helper: `voicebin.py`

The examples below use this module to read and write voices files. It needs only the Python standard library. Save it as `voicebin.py` next to the script that imports it.

```python
# voicebin.py: read and write kokoro.cpp voices files (docs/adding-voices.md).
import struct
import sys
from array import array

ROWS, DIM = 510, 256


def load(path):
    """Returns {name: array('f')} for every voice in a voices file."""
    voices = {}
    with open(path, "rb") as f:
        if f.read(4) != b"VOIC":
            raise ValueError(f"{path}: not a kokoro.cpp voices file")
        version, count = struct.unpack("<II", f.read(8))
        if version != 1:
            raise ValueError(f"{path}: unsupported voices file version {version}")
        for _ in range(count):
            (name_len,) = struct.unpack("<I", f.read(4))
            name = f.read(name_len).decode("utf-8")
            (n,) = struct.unpack("<I", f.read(4))
            style = array("f")
            style.frombytes(f.read(4 * n))
            if sys.byteorder == "big":
                style.byteswap()
            voices[name] = style
    return voices


def save(path, voices):
    """Writes {name: sequence of 510 * 256 floats} as a voices file."""
    with open(path, "wb") as f:
        f.write(b"VOIC" + struct.pack("<II", 1, len(voices)))
        for name, style in voices.items():
            data = array("f", style)
            if len(data) != ROWS * DIM:
                raise ValueError(f"{name}: expected {ROWS * DIM} floats, got {len(data)}")
            if sys.byteorder == "big":
                data.byteswap()
            encoded = name.encode("utf-8")
            f.write(struct.pack("<I", len(encoded)) + encoded + struct.pack("<I", len(data)))
            f.write(data.tobytes())
```

## 1. Blend existing voices

A weighted average of two or more voices gives a new voice somewhere between them. This is the same operation as upstream Kokoro's `KPipeline.load_voice("a,b")`, which takes the plain mean. Run from the repository root:

```python
from array import array

import voicebin


def blend(voices, weights):
    """Weighted average of voices, e.g. {"af_maple": 0.7, "bf_vale": 0.3}."""
    total = sum(weights.values())
    out = array("f", bytes(4 * voicebin.ROWS * voicebin.DIM))
    for name, weight in weights.items():
        for i, x in enumerate(voices[name]):
            out[i] += x * weight / total
    return out


voices = voicebin.load("models/voices-v1.1-zh.bin")
voices["af_maplevale"] = blend(voices, {"af_maple": 0.7, "bf_vale": 0.3})
voicebin.save("models/voices-custom.bin", voices)
```

The output file keeps every original voice and adds the new one, so it can replace the default file. Don't overwrite the downloaded `voices-v1.1-zh.bin`; keep it as the source.

Voices can be blended across languages (for example `zf_002` with `af_maple`), but the result is less predictable. Listen to it before relying on it.

## 2. Import a voice tensor (`.pt`)

Upstream Kokoro and tools built on it store each voice as a PyTorch tensor of shape `(510, 1, 256)`, saved as `<name>.pt`. Importing one needs `torch` (`pip install torch`):

```python
from array import array

import torch

import voicebin

voices = voicebin.load("models/voices-v1.1-zh.bin")
tensor = torch.load("my_voice.pt", map_location="cpu", weights_only=True)
voices["af_myvoice"] = array("f", tensor.float().flatten().tolist())
voicebin.save("models/voices-custom.bin", voices)
```

Where `.pt` voices come from:
- **[Kokoro-82M-v1.1-zh `voices/`](https://huggingface.co/hexgrad/Kokoro-82M-v1.1-zh/tree/main/voices).** These are the voices made for this model, and all 103 are already in the bundled voices file.
- **Other Kokoro releases,** such as the v1.0 voices (`af_heart`, `am_adam`, …) in [hexgrad/Kokoro-82M](https://huggingface.co/hexgrad/Kokoro-82M/tree/main/voices). They have the same shape and load without errors, but they were made for different model weights. Expect the result to sound different from the upstream samples, and listen before relying on them.

If you have a whole voice set as a NumPy file (a `.npy` dict or a `.npz` mapping names to arrays), `scripts/export_voices.py <voices.npy> <output.bin>` converts all of it in one go. It needs `numpy`, and it writes only the voices in its input, so include the bundled ones if you want to keep them.

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
