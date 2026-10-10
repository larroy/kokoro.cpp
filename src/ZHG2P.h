#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <utility>
#include "Utils.h"
#include "ZHFrontend.h"
#include "EnG2P.h"
#include "NumberNormalizer.h"

// ZHG2P: grapheme-to-phoneme frontend for mixed Chinese/English text.
// 中英混合文本的音素前端：文本 -> Kokoro 模型使用的 IPA 音素串。
//
// Pipeline (operator()):
//   1. normalize_numbers()  digits/units -> words, language from set_number_language()
//   2. map_punctuation()    full-width CJK punctuation -> ASCII + trailing space, then trim
//   3. Two paths, chosen by constructor `version`:
//      - version "1.1": ZHFrontend tokens (jieba segmentation + tone sandhi + erhua)
//          tag "eng": English word; phonemes hold the raw word, converted by EnG2P
//                     (CMU dict, user dict overrides, optional neural OOV fallback)
//          tag "x":   punctuation / unknown; phonemes pass through unchanged
//          otherwise: Chinese word; phonemes are pinyin pieces ("z", "hong1") that
//                     accumulate until one carries a tone digit, then py2ipa() per syllable
//      - any other version: legacy_call() — jieba cut, word_to_pinyin, py2ipa per syllable
//
// The return value is (IPA phonemes, extra info); extra info is always "" (kept for parity
// with the Python API).
// 返回 (IPA 音素串, 附加信息)；附加信息恒为空串，仅为对齐 Python API。
//
// Not thread-safe: processor, frontend, eng_g2p and number_language_ are read/written
// without synchronization.
// 非线程安全：processor、frontend、eng_g2p、number_language_ 的读写均无同步。
class ZHG2P {
public:
    // processor: jieba-backed segmentation + pinyin provider (shared with other frontends).
    //            分词与拼音提供者，与其他前端共享。
    // version: "1.1" builds the ZHFrontend path; any other value falls back to legacy_call().
    //          "1.1" 启用 ZHFrontend 路径，其他值回退到 legacy_call()。
    // unk: placeholder ZHFrontend emits for words without a pronunciation. Default "<unk>".
    //      ZHFrontend 对无法注音的词输出的占位符，默认 "<unk>"。
    // eng_dict_path: CMU pronouncing dictionary for English words; empty = no dictionary.
    //                英文 CMU 词典路径；为空则不加载词典。
    // eng_user_dict_path: optional CMU-format entries overriding the CMU dict.
    //                     可选的 CMU 格式用户词典，条目覆盖 CMU 词典。
    // eng_neural_model_path: optional g2p_en ONNX model for out-of-vocabulary English words.
    //                        可选的 g2p_en ONNX 模型，兜底词典未收录的英文词。
    ZHG2P(std::shared_ptr<TextProcessor> processor, const std::string& version = "1.1", const std::string& unk = "<unk>",
          const std::string& eng_dict_path = "", const std::string& eng_user_dict_path = "",
          const std::string& eng_neural_model_path = "");

    // Main entry: text -> (IPA phoneme string, extra info). Empty input yields {"", ""}.
    // Spacing rule of the 1.1 path: a space is inserted before and after English runs so
    // English words stay isolated while Chinese syllables stay glued together.
    // 主入口：文本 -> (IPA 音素串, 附加信息)。空输入返回 {"", ""}。
    // 1.1 路径的空格规则：英文词前后补空格使其独立，中文音节之间不加空格。
    std::pair<std::string, std::string> operator()(const std::string& text);

    // retone: compress IPA tone contours to Kokoro's arrow marks
    // (˧˩˧ -> ↓, ˧˥ -> ↗, ˥˩ -> ↘, ˥ -> →) and fold rhotic syllabic consonants
    // (ɻ̩, ɹ̩ and their combining variants) into ɨ.
    // 将 IPA 调值轮廓压缩为箭头记号，并把卷舌成节辅音（ɻ̩、ɹ̩ 等）折叠为 ɨ。
    static std::string retone(std::string p);

    // py2ipa: one tone-numbered pinyin syllable ("hong1", "zi4") -> IPA via parse_pinyin +
    // pinyin_to_ipa_convert + retone(). Unrecognized finals pass through verbatim; tone 5
    // (neutral) adds no tone mark.
    // 单个带调号拼音音节转 IPA。未识别的韵母原样保留；轻声（5 声）不加调号。
    static std::string py2ipa(const std::string& py);

    // map_punctuation: full-width CJK punctuation -> ASCII equivalents with a trailing space
    // (，-> ", ", 。-> ". ", 《 -> " “", …), curly quotes straightened, ends trimmed.
    // 全角中文标点转 ASCII 等价物（补尾随空格），弯引号转直引号，并去除首尾空白。
    static std::string map_punctuation(std::string text);

    // legacy_call: pre-1.1 path. jieba cut; Chinese words -> word_to_pinyin -> py2ipa per
    // syllable plus a trailing space; other tokens copied verbatim. Strips U+032F at the end.
    // 旧版路径：结巴分词后，中文词逐音节转 IPA（词尾补空格），非中文词原样拼接，最后去掉 U+032F。
    std::string legacy_call(const std::string& text);

    // Split one pinyin syllable into initial/final/tone.
    // 将单个拼音音节拆分为 (声母, 韵母, 调号)。
    // - Accepts marked vowels ("zhōng") and tone digits ("zhong1"); an explicit digit wins.
    //   同时支持标调字母（"zhōng"）与数字调号（"zhong1"）；数字调号优先。
    // - Tone defaults to 5 (neutral).
    //   缺省调号为 5（轻声）。
    // - Normalizes v -> ü and rewrites y/w onsets to i/u/ü (yi->i, yu->ü, wu->u, wa->ua, …).
    //   归一化 v -> ü，并把 y/w 开头改写为 i/u/ü 开头（yi->i、yu->ü、wu->u、wa->ua 等）。
    // - "zh"/"ch"/"sh" are matched before single-letter initials.
    //   先匹配 zh/ch/sh，再匹配单字母声母。
    struct PinyinParts {
        std::string initial;  // 声母; "" for zero-initial syllables (零声母为空串)
        std::string final;    // 韵母; may end in 'R' marking erhua (儿化韵以 R 结尾)
        int tone;             // 1-5; 5 = neutral tone (轻声)
    };
    static PinyinParts parse_pinyin(const std::string& pinyin);

    // is_chinese: true if any byte is in 0xE4..0xE9 (UTF-8 lead bytes of CJK U+4E00..U+9FFF).
    // Heuristic only: misses other CJK planes. Only used by legacy_call().
    // 粗略判断文本是否含中文：仅检查 UTF-8 首字节 0xE4..0xE9（基本区汉字），不覆盖其他 CJK 区；
    // 仅被 legacy_call() 使用。
    bool is_chinese(const std::string& str);

    // Select the number-normalization language used by operator(); Auto detects per text.
    // 设置 operator() 中数字归一化的语言；Auto 时按文本自动判断。
    void set_number_language(NumberLanguage language) { number_language_ = language; }

private:
    std::string version;   // "1.1" enables the ZHFrontend path ("1.1" 时启用 ZHFrontend 路径)
    std::string unk;       // unknown-word placeholder passed to ZHFrontend (未登录词占位符)
    std::shared_ptr<TextProcessor> processor;  // jieba segmentation + pinyin (分词与拼音)
    std::unique_ptr<ZHFrontend> frontend;      // non-null only for version "1.1" (仅 1.1 版非空)
    std::unique_ptr<EnG2P> eng_g2p;            // CMU dict + optional neural OOV fallback (英文 G2P)
    NumberLanguage number_language_ = NumberLanguage::Auto;

    // pinyin syllable -> IPA segments: initial + final with tone marks applied (the "0"
    // placeholder in final mappings is replaced by the tone contour), erhua final appends ɚ.
    // No retone(); callers apply it separately.
    // 拼音音节 -> IPA 片段：声母 + 韵母（映射表中的 "0" 占位符替换为调值轮廓），儿化韵追加 ɚ。
    // 不含 retone()，由调用方另行处理。
    static std::string pinyin_to_ipa_convert(const std::string& pinyin);
};
