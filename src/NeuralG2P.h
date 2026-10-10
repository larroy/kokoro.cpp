#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// Symbol tables of one trained g2p checkpoint; index order must match its embeddings.
struct G2PSymbols {
    std::vector<std::string> graphemes;  // [0..2] = <pad>, <unk>, </s>; then one UTF-8 character each
    std::vector<std::string> phonemes;   // [0..3] = <pad>, <unk>, <s>, </s>; then the model's phonemes
};

// C++ port of the GRU seq2seq model shared by g2p_en (https://github.com/Kyubyong/g2p, see
// dict/g2p_en.LICENSE.txt) and g2p_de (https://github.com/gooofy/g2p_de, see dict/g2p_de.LICENSE.txt),
// both Apache-2.0. Predicts pronunciations for words missing from a dictionary. Weights are produced
// by scripts/export_g2p.py.
class NeuralG2P {
public:
    explicit NeuralG2P(G2PSymbols symbols);

    // Returns false (and stays unloaded) if the file is missing or malformed.
    bool load(const std::string& path);
    bool loaded() const { return hidden_ > 0; }

    // word: lowercase UTF-8; characters missing from symbols().graphemes are fed as <unk>.
    std::vector<std::string> predict(const std::string& word) const;
    const G2PSymbols& symbols() const { return symbols_; }

private:
    G2PSymbols symbols_;
    std::unordered_map<std::string, int> grapheme_ids_;  // graphemes[i] -> i for i >= 3
    int hidden_ = 0;
    // Embedding rows pre-multiplied by W_ih plus b_ih: one 3*hidden row per input symbol.
    std::vector<float> enc_in_, dec_in_;
    std::vector<float> enc_w_hh_, enc_b_hh_, dec_w_hh_, dec_b_hh_;
    std::vector<float> fc_w_, fc_b_;

    void gru_step(const float* gi, const std::vector<float>& w_hh, const std::vector<float>& b_hh,
                  std::vector<float>& h, std::vector<float>& gh) const;
};
