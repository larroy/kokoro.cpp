# Larroy.Kokoro

.NET bindings for [kokoro.cpp](https://github.com/larroy/kokoro.cpp), a C++ runtime for the Kokoro text-to-speech
model on ONNX Runtime.

## Install

```sh
dotnet add package Larroy.Kokoro
```

`kokoro.dll` for win-x64 and win-arm64 arrives through the `Larroy.Kokoro.runtime.win-x64` and
`Larroy.Kokoro.runtime.win-arm64` packages. ONNX Runtime is not bundled: those packages depend on
`Microsoft.ML.OnnxRuntime` 1.23.2 or newer, so an app that also references `Microsoft.ML.OnnxRuntime` gets one
`onnxruntime.dll`, the highest version requested, and kokoro uses it.

## Usage

```csharp
using Kokoro.Net;

using var ctx = new KokoroContext(
    "models/kokoro-v1.1-zh.onnx", "models/voices-v1.1-zh.bin", KokoroContext.BundledDictDirectory,
    new KokoroOptions(KokoroDevice.Cpu, 0));
KokoroAudio audio = ctx.Synthesize("Hello world.", "af_maple");
// audio.Samples: mono float PCM; audio.SampleRate: 24000 (24 kHz).
```

Failures throw `KokoroException`; its `Status` mirrors the C API status code.

## Model files are not included

The model and the voices file are passed by path; no package contains them. Get them by running
`uv run bootstrap.py configure` in the kokoro.cpp repository, or download the release URLs it pins.

The G2P dictionaries are tied to the native code version, so `Larroy.Kokoro` ships them and copies them to
`<output>/kokoro-dict` on build and publish. Pass `KokoroContext.BundledDictDirectory` as `dictDir`.

## Threading

Separate contexts are independent and may be used from different threads. A single context must not be used by two
threads at the same time.

## CUDA (win-x64)

- Add `Larroy.Kokoro.runtime.win-x64.cuda`; `KokoroDevice.Auto` then uses CUDA. It brings
  `Microsoft.ML.OnnxRuntime.Gpu.Windows` and makes its CUDA build of `onnxruntime.dll` take precedence over the CPU
  build from `Microsoft.ML.OnnxRuntime`.
- If the app references a newer `Microsoft.ML.OnnxRuntime`, also reference `Microsoft.ML.OnnxRuntime.Gpu.Windows` at
  the same version; otherwise the newer CPU `onnxruntime.dll` wins and CUDA is unavailable.
- The CUDA 12 / cuDNN 9 runtime DLLs must be on `PATH`: `cudart64_12.dll`, `cublas64_12.dll`, `cublasLt64_12.dll`,
  `curand64_10.dll`, `cufft64_11.dll`, `cudnn64_9.dll`.
- Needs an NVIDIA driver supporting CUDA 12.8 or newer. The provider contains code for sm 7.5, 8.6 and 8.9; newer
  GPUs run through PTX 9.0.
- Without the add-on `onnxruntime.dll` is the CPU build: `KokoroDevice.Auto` runs on the CPU and `KokoroDevice.Cuda`
  fails.

## VC++ runtime (required)

`kokoro.dll` and `onnxruntime.dll` need the MSVC C++ runtime 14.38 or newer (an ONNX Runtime 1.20+ requirement).
The packages deliberately do not include it.

- Ship `msvcp140.dll`, `msvcp140_1.dll`, `vcruntime140.dll` and `vcruntime140_1.dll` (from the VC++ 2015-2022
  redistributable, 14.38 or newer) app-local, in the same directory as `kokoro.dll`. That is the app directory when
  building or publishing with `-r win-x64` / `-r win-arm64`, and `runtimes/<rid>/native/` for RID-less builds.
- Alternatively, require the VC++ redistributable 14.38 or newer installed system-wide. Older copies in System32
  crash ONNX Runtime.
