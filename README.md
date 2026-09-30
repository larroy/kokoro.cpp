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

This produces the shared library `kokoro` (`kokoro.dll` / `libkokoro.so` / `libkokoro.dylib`) and the `kokoro` command-line tool. On Windows the bundled static ONNX Runtime is linked into `kokoro.dll`, so the DLL has no other runtime dependencies. On Linux/macOS, `libkokoro` links against the ONNX Runtime shared library, which must be findable at runtime.

### Tests

```bash
ctest --test-dir build -C Release --output-on-failure
```

Tests live in `tests/` (doctest, vendored in `third_party/doctest`): `c_api` checks the C API without a model, `g2p` pins the Chinese/English G2P output, and `synthesis` runs the real model from `models/` (skipped if the model or voices file is missing). Configure with `-DKOKORO_BUILD_TESTS=OFF` to skip building them.

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

Run the `kokoro` command-line tool from the project root (the defaults point at `models/` and `dict/`). With Visual Studio or another multi-config generator the executable is in `build/Release/`.

```bash
./build/kokoro [options] <text>
./build/kokoro --list-voices
```

| Option | Meaning |
|---|---|
| `-m, --model <path>` | ONNX model file (default: `models/kokoro-v1.1-zh.onnx`) |
| `--voices <path>` | voices file (default: `models/voices-v1.1-zh.bin`) |
| `-d, --dict <dir>` | dictionary directory (default: `dict`) |
| `-v, --voice <name>` | voice (default: `zf_002`); English voices: `af_maple`, `af_sol`, `bf_vale` |
| `-s, --speed <rate>` | speaking rate, > 0 (default: `1.0`) |
| `-o, --output <path>` | output WAV file (default: `output.wav`) |
| `-p, --phonemes` | `<text>` is a phoneme string; skip G2P |
| `--phonemize` | print the phonemes for `<text>` instead of synthesizing |
| `--list-voices` | print the available voices |

### Example

```bash
./build/kokoro "你好啊，这是一个测试。Hello world"
./build/kokoro --voice af_maple -o hello.wav "Hello world"
./build/kokoro --phonemize "中国"
```

Output is a mono 32-bit float WAV at 24 kHz. Exit status is 0 on success, 1 on a library error, and 2 on a usage error.

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

- `include/kokoro/kokoro.h`, `src/kokoro_c.cpp`: public C API of `libkokoro`.
- `src/cli/main.cpp`: the `kokoro` command-line tool.
- `src/Kokoro.cpp/h`: main TTS class (internal).
- `src/ZHFrontend.cpp/h`: Chinese frontend (G2P, tone sandhi).
- `src/EnG2P.h`, `src/NeuralG2P.cpp/h`: English G2P (dictionary lookup and neural prediction).
- `tests/`: library tests (run with `ctest`).
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

编译产物为共享库 `kokoro`（`kokoro.dll` / `libkokoro.so` / `libkokoro.dylib`）以及 `kokoro` 命令行工具。Windows 下内置的静态 ONNX Runtime 会被链接进 `kokoro.dll`，DLL 不依赖其他运行库；Linux/macOS 下 `libkokoro` 链接 ONNX Runtime 共享库，运行时需能找到它。

### 测试

```bash
ctest --test-dir build -C Release --output-on-failure
```

测试位于 `tests/`（使用 doctest，已内置于 `third_party/doctest`）：`c_api` 在无模型的情况下检查 C API，`g2p` 固定中英文 G2P 的输出，`synthesis` 使用 `models/` 中的真实模型运行（模型或语音文件不存在时跳过）。配置时传入 `-DKOKORO_BUILD_TESTS=OFF` 可不编译测试。

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

在项目根目录运行 `kokoro` 命令行工具（默认路径指向 `models/` 和 `dict/`）。使用 Visual Studio 等多配置生成器时，可执行文件位于 `build/Release/`。

```bash
./build/kokoro [选项] <文本>
./build/kokoro --list-voices
```

| 选项 | 含义 |
|---|---|
| `-m, --model <路径>` | ONNX 模型文件（默认：`models/kokoro-v1.1-zh.onnx`） |
| `--voices <路径>` | 语音文件（默认：`models/voices-v1.1-zh.bin`） |
| `-d, --dict <目录>` | 词典目录（默认：`dict`） |
| `-v, --voice <名称>` | 语音（默认：`zf_002`）；英文语音：`af_maple`、`af_sol`、`bf_vale` |
| `-s, --speed <语速>` | 语速，须 > 0（默认：`1.0`） |
| `-o, --output <路径>` | 输出 WAV 文件（默认：`output.wav`） |
| `-p, --phonemes` | `<文本>` 为音素串，跳过 G2P |
| `--phonemize` | 输出 `<文本>` 的音素而不合成 |
| `--list-voices` | 列出可用语音 |

### 示例

```bash
./build/kokoro "你好啊，这是一个测试。Hello world"
./build/kokoro --voice af_maple -o hello.wav "Hello world"
./build/kokoro --phonemize "中国"
```

输出为 24 kHz 单声道 32 位浮点 WAV。成功时退出码为 0，库错误为 1，用法错误为 2。

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

- `include/kokoro/kokoro.h`, `src/kokoro_c.cpp`: `libkokoro` 的公共 C API。
- `src/cli/main.cpp`: `kokoro` 命令行工具。
- `src/Kokoro.cpp/h`: 主要的 TTS 类（内部实现）。
- `src/ZHFrontend.cpp/h`: 中文前端（G2P、变调）。
- `src/EnG2P.h`, `src/NeuralG2P.cpp/h`: 英文 G2P（词典查询与神经网络预测）。
- `tests/`: 库测试（使用 `ctest` 运行）。
- `scripts/`: 数据处理辅助脚本。
- `dict/`: G2P 字典文件（Jieba、拼音、CMU、g2p_en 权重）。

## 许可证

MIT
