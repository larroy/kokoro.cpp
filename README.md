# Kokoro C++ Inference

English | [中文](#kokoro-c-推理)

A high-performance, lightweight C++ inference implementation of the [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M) TTS model, built on ONNX Runtime. The project currently supports mixed **Chinese and English** synthesis.

## Features

- 🚀 **Fast inference**: powered by ONNX Runtime.
- 🌏 **Bilingual**: native support for Chinese and English.

## Requirements

- **CMake** (3.15+)
- **Ninja** (recommended; `python setup.py build` uses it when it is in `PATH`)
- **C++ compiler** (C++17 support required)
- **Python 3** with [click](https://click.palletsprojects.com/) (`pip install click`) for `setup.py`

## Setup

```bash
python setup.py configure
```

This downloads, verifying SHA-256 checksums:

- the prebuilt [ONNX Runtime](https://github.com/microsoft/onnxruntime/releases/tag/v1.23.2) 1.23.2 (CPU) for the current platform (Linux x64/aarch64, macOS arm64/x86_64, Windows x64/arm64) into `third_party/onnxruntime/`;
- the model `kokoro-v1.1-zh.onnx` and the voice pack `voices-v1.1-zh.bin` from this repository's [`voices_model_files` release](https://github.com/larroy/kokoro.cpp/releases/tag/voices_model_files) into `models/`.

Files that are already present and up to date are skipped; `--force` downloads them again. The ONNX Runtime version and checksums are pinned in `setup.py`. To use another ONNX Runtime installation instead, skip `configure` and pass `-DONNXRUNTIME_ROOT=/path/to/onnxruntime` to CMake.

## Building

```bash
python setup.py build
```

`build` runs the CMake configure and build steps with the Ninja generator. If `ninja` is not in `PATH` it prints a warning and falls back to CMake's default generator; passing `-G` after `--` overrides the choice. Options: `--build-dir` (default `build`), `--config` (`Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`; default `Release`), `-j/--jobs`. Arguments after `--` go to the CMake configure step, e.g. `python setup.py build -- -DKOKORO_BUILD_TESTS=OFF`. A build directory can't switch generators: delete it first if it was configured with another one. On Windows, Ninja needs the MSVC environment (run from a Developer Command Prompt). It is equivalent to:

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

This produces the shared library `kokoro` (`kokoro.dll` / `libkokoro.so` / `libkokoro.dylib`) and the `kokoro` command-line tool. `libkokoro` links against the ONNX Runtime shared library. On Linux/macOS the build tree finds it through the rpath; installed copies need it findable at runtime. On Windows `onnxruntime.dll` is copied next to `kokoro.dll` (and installed with it).

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
| `-v, --voice <name>` | voice (default: `af_maple`); English voices: `af_maple`, `af_sol`, `bf_vale`; Chinese voices: `zf_*`, `zm_*` |
| `-s, --speed <rate>` | speaking rate, > 0 (default: `1.0`) |
| `-o, --output <path>` | output WAV file (default: `output.wav`) |
| `-p, --phonemes` | `<text>` is a phoneme string; skip G2P |
| `--phonemize` | print the phonemes for `<text>` instead of synthesizing |
| `--list-voices` | print the available voices |

To add your own voices (blends, imported `.pt` tensors), see [docs/adding-voices.md](docs/adding-voices.md).

### Example

```bash
./build/kokoro -o hello.wav "Hello world"
./build/kokoro --voice zf_002 "你好啊，这是一个测试。Hello world"
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
- `docs/`: guides ([adding voices](docs/adding-voices.md)).
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

- **CMake** (3.15+)
- **Ninja**（推荐；`python setup.py build` 在 `PATH` 中找到它时使用）
- **C++ 编译器** (需要支持 C++17)
- **Python 3** 及 [click](https://click.palletsprojects.com/)（`pip install click`），用于 `setup.py`

## 准备

```bash
python setup.py configure
```

该命令会下载以下文件并校验 SHA-256：

- 当前平台（Linux x64/aarch64、macOS arm64/x86_64、Windows x64/arm64）的预编译 [ONNX Runtime](https://github.com/microsoft/onnxruntime/releases/tag/v1.23.2) 1.23.2（CPU 版），放入 `third_party/onnxruntime/`；
- 模型 `kokoro-v1.1-zh.onnx` 和语音包 `voices-v1.1-zh.bin`，来自本仓库的 [`voices_model_files` release](https://github.com/larroy/kokoro.cpp/releases/tag/voices_model_files)，放入 `models/`。

已存在且校验一致的文件会被跳过；`--force` 强制重新下载。ONNX Runtime 版本和校验值固定在 `setup.py` 中。如需使用其他 ONNX Runtime，可跳过 `configure`，并向 CMake 传入 `-DONNXRUNTIME_ROOT=/path/to/onnxruntime`。

## 编译

```bash
python setup.py build
```

`build` 使用 Ninja 生成器执行 CMake 的配置和编译。若 `PATH` 中没有 `ninja`，会打印警告并回退到 CMake 默认生成器；在 `--` 之后传入 `-G` 可覆盖该选择。选项：`--build-dir`（默认 `build`）、`--config`（`Debug`、`Release`、`RelWithDebInfo`、`MinSizeRel`，默认 `Release`）、`-j/--jobs`。`--` 之后的参数会传给 CMake 配置步骤，例如 `python setup.py build -- -DKOKORO_BUILD_TESTS=OFF`。构建目录不能更换生成器：若之前用其他生成器配置过，需先删除。Windows 下 Ninja 需要 MSVC 环境（在 Developer Command Prompt 中运行）。等价于：

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

编译产物为共享库 `kokoro`（`kokoro.dll` / `libkokoro.so` / `libkokoro.dylib`）以及 `kokoro` 命令行工具。`libkokoro` 链接 ONNX Runtime 共享库：Linux/macOS 下构建目录中通过 rpath 找到它，安装后运行时需能找到它；Windows 下 `onnxruntime.dll` 会被复制到 `kokoro.dll` 旁边（并随之安装）。

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
| `-v, --voice <名称>` | 语音（默认：`af_maple`）；英文语音：`af_maple`、`af_sol`、`bf_vale`；中文语音：`zf_*`、`zm_*` |
| `-s, --speed <语速>` | 语速，须 > 0（默认：`1.0`） |
| `-o, --output <路径>` | 输出 WAV 文件（默认：`output.wav`） |
| `-p, --phonemes` | `<文本>` 为音素串，跳过 G2P |
| `--phonemize` | 输出 `<文本>` 的音素而不合成 |
| `--list-voices` | 列出可用语音 |

如需添加自定义语音（混合语音、导入 `.pt` 张量），请参阅 [docs/adding-voices.md](docs/adding-voices.md)（英文）。

### 示例

```bash
./build/kokoro -o hello.wav "Hello world"
./build/kokoro --voice zf_002 "你好啊，这是一个测试。Hello world"
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
<<<<<<< HEAD
- `setup.py`: 下载依赖（`configure`）并运行 CMake（`build`）。
=======
- `docs/`: 使用指南（[添加语音](docs/adding-voices.md)）。
>>>>>>> 5c019f4 (docs: add guide for adding voices)
- `scripts/`: 数据处理辅助脚本。
- `dict/`: G2P 字典文件（Jieba、拼音、CMU、g2p_en 权重）。

## 许可证

MIT
