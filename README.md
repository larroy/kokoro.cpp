# Kokoro C++ Inference

English | [中文](#kokoro-c-推理)

A high-performance, lightweight C++ inference implementation of the [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M) TTS model, built on ONNX Runtime. The project currently supports mixed **Chinese and English** synthesis.

## Features

- 🚀 **Fast inference**: powered by ONNX Runtime.
- 🌏 **Bilingual**: native support for Chinese and English.

## Requirements

- **CMake** (3.14+)
- **C++ compiler** (C++17 support required)
- **Optional: Python 3** (for the data preparation scripts)

## Data Preparation

Before building, you need to prepare the model and voice files.

### 1. Download the voice pack

This project stores voice styles in a compact binary format. You need to download the voice data.

Download `voices-v1.1-zh.bin` from [here](https://github.com/koth/kokoro.cpp/releases/download/voices_model_files/voices-v1.1-zh.bin).

### 2. Download the model

Download the ONNX model file.

Download `kokoro-v1.1-zh.onnx` from [here](https://github.com/koth/kokoro.cpp/releases/download/voices_model_files/kokoro-v1.1-zh.onnx).

## Building

```bash
cmake -B build -S .
cmake --build build --config Release
```

This produces the shared library `kokoro` (`kokoro.dll` / `libkokoro.so` / `libkokoro.dylib`) and the `kokoro_demo` / `zhg2p_demo` executables. On Windows the bundled static ONNX Runtime is linked into `kokoro.dll`, so the DLL has no other runtime dependencies. On Linux/macOS, `libkokoro` links against the ONNX Runtime shared library, which must be findable at runtime.

## Library

`libkokoro` exposes a C API in [`include/kokoro/kokoro.h`](include/kokoro/kokoro.h), so it can be used from C, C++ or any language with a C FFI (Python `ctypes`, C#, Rust, Go, ...):

```c
#include <kokoro/kokoro.h>

kokoro_ctx* ctx = NULL;
if (kokoro_create("models/kokoro-v1.1-zh.onnx", "models/voices-v1.1-zh.bin", "dict", &ctx) != KOKORO_OK) {
    fprintf(stderr, "%s\n", kokoro_last_error());
    return 1;
}
kokoro_audio audio;
if (kokoro_synthesize(ctx, "你好，世界。Hello world", "zf_002", 1.0f, 0, &audio) == KOKORO_OK) {
    /* audio.samples: mono float PCM, audio.num_samples samples at audio.sample_rate Hz */
    kokoro_audio_free(&audio);
}
kokoro_destroy(ctx);
```

- All strings, including paths, are UTF-8. `dict_dir` is the directory holding `vocab.txt` and the G2P dictionaries (`dict/` in this repo).
- Errors are reported as `kokoro_status` codes; `kokoro_last_error()` returns the message for the calling thread.
- `kokoro_voice_count` / `kokoro_voice_name` enumerate voices, `kokoro_phonemize` returns the phoneme string, and `KOKORO_INPUT_PHONEMES` synthesizes phonemes directly.
- Separate contexts can be used from different threads; a single context must not be used concurrently.

Install it and consume it from CMake:

```bash
cmake --install build --config Release --prefix /path/to/prefix
```

```cmake
find_package(kokoro REQUIRED)   # with CMAKE_PREFIX_PATH=/path/to/prefix
target_link_libraries(app PRIVATE kokoro::kokoro)
```

Alternatively, `add_subdirectory(kokoro.cpp)` and link `kokoro::kokoro`.

## Usage

Run the `kokoro_demo` executable, specifying the model, the voice file, and the input text.

```bash
./kokoro_demo <model_path> <voices_path> <"text to speak"> [dict_dir] [voice_name]
```

`dict_dir` defaults to `dict` and the voice name to `zf_002`. English voices: `af_maple`, `af_sol`, `bf_vale`. For example:

```bash
./build/kokoro_demo models/kokoro-v1.1-zh.onnx models/voices-v1.1-zh.bin "Hello world" dict af_maple
```

### Example

```bash
./build/kokoro_demo models/kokoro-v1.1-zh.onnx models/voices-v1.1-zh.bin "你好啊，这是一个测试。Hello world"
```

Run it from the project root or pass `dict_dir` explicitly. The output audio is saved as `output.wav` in the current directory.

## English G2P

English words are converted to phonemes in the following order (output uses the Kokoro/misaki English phoneme set):

1. `dict/user_en.dict`: user dictionary in CMU format (`WORD ARPAbet-phonemes`, e.g. `onnx AA1 N IH0 K S`). Highest priority; intended for proper nouns.
2. `dict/cmudict-0.7b/cmudict.dict`: the CMU Pronouncing Dictionary.
3. All-uppercase words of at most 5 letters (e.g. `GPU`) are spelled out by English letter names.
4. Other out-of-vocabulary words are predicted by the `dict/g2p_en.weights` neural network, ported from [g2p_en](https://github.com/Kyubyong/g2p) (Apache-2.0, see `dict/g2p_en.LICENSE.txt`).

`g2p_en.weights` is generated from g2p_en's `checkpoint20.npz` (numpy not required):

```bash
python scripts/export_g2p_en.py checkpoint20.npz dict/g2p_en.weights
```

## Project Structure

- `include/kokoro/kokoro.h`, `kokoro_c.cpp`: public C API of `libkokoro`.
- `Kokoro.cpp/h`: main TTS class (internal).
- `ZHFrontend.cpp/h`: Chinese frontend (G2P, tone sandhi).
- `EnG2P.h`, `NeuralG2P.cpp/h`: English G2P (dictionary lookup and neural prediction).
- `scripts/`: helper scripts for data processing.
- `dict/`: G2P dictionary files (Jieba, pinyin, CMU, g2p_en weights).

## License

MIT

---

# Kokoro C++ 推理

[English](#kokoro-c-inference) | 中文

基于 ONNX Runtime 的 [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M) TTS 模型的高性能轻量级 C++ 推理实现。本项目目前支持**中英文**混合合成。

## 特性

- 🚀 **快速推理**：由 ONNX Runtime 驱动。
- 🌏 **双语支持**：原生支持中文和英文。

## 依赖环境

- **CMake** (3.14+)
- **C++ 编译器** (需要支持 C++17)
- **可选： Python 3** (用于数据准备脚本)

## 数据准备

在编译之前，你需要准备模型和语音文件。

### 1. 下载语音包

本项目使用紧凑的二进制格式存储语音风格。你需要下载语音数据。

请从[这里](https://github.com/koth/kokoro.cpp/releases/download/voices_model_files/voices-v1.1-zh.bin)下载 `voices-v1.1-zh.bin`。

### 2. 下载模型

下载 ONNX 模型文件。

请从[这里](https://github.com/koth/kokoro.cpp/releases/download/voices_model_files/kokoro-v1.1-zh.onnx)下载 `kokoro-v1.1-zh.onnx`。

## 编译

```bash
cmake -B build -S .
cmake --build build --config Release
```

编译产物为共享库 `kokoro`（`kokoro.dll` / `libkokoro.so` / `libkokoro.dylib`）以及 `kokoro_demo` / `zhg2p_demo` 可执行文件。Windows 下内置的静态 ONNX Runtime 会被链接进 `kokoro.dll`，DLL 不依赖其他运行库；Linux/macOS 下 `libkokoro` 链接 ONNX Runtime 共享库，运行时需能找到它。

## 库

`libkokoro` 在 [`include/kokoro/kokoro.h`](include/kokoro/kokoro.h) 中提供 C API，可在 C、C++ 以及任何支持 C FFI 的语言（Python `ctypes`、C#、Rust、Go 等）中使用：

```c
#include <kokoro/kokoro.h>

kokoro_ctx* ctx = NULL;
if (kokoro_create("models/kokoro-v1.1-zh.onnx", "models/voices-v1.1-zh.bin", "dict", &ctx) != KOKORO_OK) {
    fprintf(stderr, "%s\n", kokoro_last_error());
    return 1;
}
kokoro_audio audio;
if (kokoro_synthesize(ctx, "你好，世界。Hello world", "zf_002", 1.0f, 0, &audio) == KOKORO_OK) {
    /* audio.samples：单声道 float PCM，共 audio.num_samples 个采样，采样率 audio.sample_rate */
    kokoro_audio_free(&audio);
}
kokoro_destroy(ctx);
```

- 所有字符串（包括路径）均为 UTF-8。`dict_dir` 为存放 `vocab.txt` 和 G2P 词典的目录（即本仓库的 `dict/`）。
- 错误通过 `kokoro_status` 返回；`kokoro_last_error()` 返回当前线程的错误信息。
- `kokoro_voice_count` / `kokoro_voice_name` 枚举语音，`kokoro_phonemize` 返回音素串，`KOKORO_INPUT_PHONEMES` 可直接合成音素。
- 不同的 context 可在不同线程中使用；同一个 context 不能被并发调用。

安装并在 CMake 中使用：

```bash
cmake --install build --config Release --prefix /path/to/prefix
```

```cmake
find_package(kokoro REQUIRED)   # 设置 CMAKE_PREFIX_PATH=/path/to/prefix
target_link_libraries(app PRIVATE kokoro::kokoro)
```

也可以 `add_subdirectory(kokoro.cpp)` 后链接 `kokoro::kokoro`。

## 使用方法

运行 `kokoro_demo` 可执行文件，指定模型、语音文件和输入文本。

```bash
./kokoro_demo <模型路径> <语音文件路径> <"要朗读的文本"> [词典目录] [语音名称]
```

词典目录默认为 `dict`，语音名称默认为 `zf_002`。英文语音：`af_maple`、`af_sol`、`bf_vale`，例如：

```bash
./build/kokoro_demo models/kokoro-v1.1-zh.onnx models/voices-v1.1-zh.bin "Hello world" dict af_maple
```

### 示例

```bash
./build/kokoro_demo models/kokoro-v1.1-zh.onnx models/voices-v1.1-zh.bin "你好啊，这是一个测试。Hello world"
```

请在项目根目录运行，或显式传入词典目录。输出的音频将保存为当前目录下的 `output.wav`。

## 英文 G2P

英文单词按以下顺序转换为音素（输出为 Kokoro/misaki 英文音素集）：

1. `dict/user_en.dict`：用户词典，CMU 格式（`单词 ARPAbet音素`，如 `onnx AA1 N IH0 K S`），优先级最高，用于专有名词。
2. `dict/cmudict-0.7b/cmudict.dict`：CMU 发音词典。
3. 不超过 5 个字母的全大写词（如 `GPU`）按英文字母名拼读。
4. 其他未登录词由 `dict/g2p_en.weights` 神经网络预测，移植自 [g2p_en](https://github.com/Kyubyong/g2p)（Apache-2.0，见 `dict/g2p_en.LICENSE.txt`）。

`g2p_en.weights` 由 g2p_en 的 `checkpoint20.npz` 生成（无需 numpy）：

```bash
python scripts/export_g2p_en.py checkpoint20.npz dict/g2p_en.weights
```

## 项目结构

- `include/kokoro/kokoro.h`, `kokoro_c.cpp`: `libkokoro` 的公共 C API。
- `Kokoro.cpp/h`: 主要的 TTS 类（内部实现）。
- `ZHFrontend.cpp/h`: 中文前端（G2P、变调）。
- `EnG2P.h`, `NeuralG2P.cpp/h`: 英文 G2P（词典查询与神经网络预测）。
- `scripts/`: 数据处理辅助脚本。
- `dict/`: G2P 字典文件（Jieba、拼音、CMU、g2p_en 权重）。

## 许可证

MIT
