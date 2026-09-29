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
mkdir build
cmake -B build  -S .
cmake --build build --config Release
```

## Usage

Run the `kokoro_demo` executable, specifying the model, the voice file, and the input text.

```bash
./kokoro_demo <model_path> <voices_path> <"text to speak"> [vocab_path] [voice_name]
```

The voice name defaults to `zf_002`. English voices: `af_maple`, `af_sol`, `bf_vale`. For example:

```bash
./build/kokoro_demo models/kokoro-v1.1-zh.onnx models/voices-v1.1-zh.bin "Hello world" dict/vocab.txt af_maple
```

### Example

```bash
./build/kokoro_demo models/kokoro-v1.1-zh.onnx models/voices-v1.1-zh.bin "你好啊，这是一个测试。Hello world"
```

Because it depends on the bundled dictionaries, it must be run from the project root! The output audio is saved as `output.wav` in the current directory.

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

- `Kokoro.cpp/h`: main TTS class.
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
mkdir build
cmake -B build  -S .
cmake --build build --config Release
```

## 使用方法

运行 `kokoro_demo` 可执行文件，指定模型、语音文件和输入文本。

```bash
./kokoro_demo <模型路径> <语音文件路径> <"要朗读的文本"> [词表路径] [语音名称]
```

语音名称默认为 `zf_002`。英文语音：`af_maple`、`af_sol`、`bf_vale`，例如：

```bash
./build/kokoro_demo models/kokoro-v1.1-zh.onnx models/voices-v1.1-zh.bin "Hello world" dict/vocab.txt af_maple
```

### 示例

```bash
./build/kokoro_demo models/kokoro-v1.1-zh.onnx models/voices-v1.1-zh.bin "你好啊，这是一个测试。Hello world"
```

因为依赖相关词典，需要在项目根目录运行！ 输出的音频将保存为当前目录下的 `output.wav`。

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

- `Kokoro.cpp/h`: 主要的 TTS 类。
- `ZHFrontend.cpp/h`: 中文前端（G2P、变调）。
- `EnG2P.h`, `NeuralG2P.cpp/h`: 英文 G2P（词典查询与神经网络预测）。
- `scripts/`: 数据处理辅助脚本。
- `dict/`: G2P 字典文件（Jieba、拼音、CMU、g2p_en 权重）。

## 许可证

MIT
