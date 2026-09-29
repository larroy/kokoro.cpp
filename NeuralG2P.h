#pragma once
#include <string>
#include <vector>

// C++ port of the g2p_en GRU seq2seq model (https://github.com/Kyubyong/g2p, Apache-2.0,
// see dict/g2p_en.LICENSE.txt). Predicts ARPAbet pronunciations for words missing from
// the CMU dict. Weights are produced by scripts/export_g2p_en.py.
class NeuralG2P {
public:
    // Returns false (and stays unloaded) if the file is missing or malformed.
    bool load(const std::string& path);
    bool loaded() const { return hidden_ > 0; }

    // word: lowercase; characters outside a-z are fed as <unk>. Returns ARPAbet phonemes.
    std::vector<std::string> predict(const std::string& word) const;

private:
    int hidden_ = 0;
    // Embedding rows pre-multiplied by W_ih plus b_ih: one 3*hidden row per input symbol.
    std::vector<float> enc_in_, dec_in_;
    std::vector<float> enc_w_hh_, enc_b_hh_, dec_w_hh_, dec_b_hh_;
    std::vector<float> fc_w_, fc_b_;

    void gru_step(const float* gi, const std::vector<float>& w_hh, const std::vector<float>& b_hh,
                  std::vector<float>& h, std::vector<float>& gh) const;
};
