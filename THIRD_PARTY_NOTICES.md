# Third-party notices

kokoro.cpp is MIT licensed (see `LICENSE`). It builds on, or redistributes, the components below.
Each component remains under its own license; the full license texts are in the paths listed.

## Compiled into kokoro.dll

### cppjieba

- Source: https://github.com/yanyiwu/cppjieba
- License: MIT
- License text: `third_party/cppjieba/LICENSE`
- The Chinese segmentation dictionaries in `dict/` (`jieba.dict.utf8`, `hmm_model.utf8`, `idf.utf8`,
  `stop_words.utf8`, `user.dict.utf8`, `pos_dict/`) come from cppjieba and are under the same license.

### limonp

- Source: https://github.com/yanyiwu/limonp
- License: MIT
- License text: `third_party/cppjieba/deps/limonp/LICENSE`

## Redistributed data (`dict/`, shipped as `kokoro-dict/` in the Larroy.Kokoro NuGet package)

### CMU Pronouncing Dictionary

- Source: http://www.speech.cs.cmu.edu/cgi-bin/cmudict
- License: BSD-style (Carnegie Mellon University)
- License text: `dict/cmudict-0.7b/LICENSE`

### g2p_en model weights

- Source: https://github.com/Kyubyong/g2p
- License: Apache License 2.0
- License text: `dict/g2p_en.LICENSE.txt`
- File: `dict/g2p_en.weights`

### pinyin-data

- Source: https://github.com/mozillazg/pinyin-data
- License: MIT
- Files: `dict/pinyin.txt`, `dict/pinyin_phrase.txt`

## Dependencies that are not redistributed

### ONNX Runtime

- Source: https://github.com/microsoft/onnxruntime
- License: MIT
- The NuGet packages depend on `Microsoft.ML.OnnxRuntime` (or `Microsoft.ML.OnnxRuntime.Gpu.Windows`)
  and do not ship their own copy of `onnxruntime.dll`.

### Kokoro-82M model weights and voices

- Source: https://huggingface.co/hexgrad/Kokoro-82M and https://huggingface.co/hexgrad/Kokoro-82M-v1.1-zh
- License: Apache License 2.0
- The model (`*.onnx`) and voice (`voices-*.bin`) files are not packaged; users download them separately.
