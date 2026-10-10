#include "NeuralG2P.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>

namespace {

// Special indices shared by g2p_en/g2p.py and g2p_de/g2p.py.
constexpr int GRAPHEME_UNK = 1, GRAPHEME_EOS = 2, FIRST_REAL_GRAPHEME = 3;
constexpr int PHONEME_BOS = 2, PHONEME_EOS = 3, FIRST_REAL_PHONEME = 4;
constexpr int MAX_DECODE_STEPS = 20;

struct Tensor {
    std::vector<uint32_t> shape;
    std::vector<float> data;
};

bool read_u32(std::ifstream& in, uint32_t& v) {
    return static_cast<bool>(in.read(reinterpret_cast<char*>(&v), 4));
}

bool has_shape(const Tensor& t, std::initializer_list<uint32_t> dims) {
    return t.shape == std::vector<uint32_t>(dims);
}

// out[s * rows + j] = b[j] + sum_k emb[s, k] * w[j, k]
std::vector<float> project(const Tensor& emb, const Tensor& w, const Tensor& b) {
    const uint32_t symbols = emb.shape[0], dim = emb.shape[1], rows = w.shape[0];
    std::vector<float> out(static_cast<size_t>(symbols) * rows);
    for (uint32_t s = 0; s < symbols; ++s) {
        const float* e = &emb.data[static_cast<size_t>(s) * dim];
        for (uint32_t j = 0; j < rows; ++j) {
            const float* wr = &w.data[static_cast<size_t>(j) * dim];
            float acc = b.data[j];
            for (uint32_t k = 0; k < dim; ++k) acc += e[k] * wr[k];
            out[static_cast<size_t>(s) * rows + j] = acc;
        }
    }
    return out;
}

float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

// Byte length of the UTF-8 character whose lead byte is `lead`; continuation/invalid bytes count as 1.
size_t utf8_char_length(unsigned char lead) {
    if ((lead & 0xF8) == 0xF0) return 4;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xE0) == 0xC0) return 2;
    return 1;
}

}  // namespace

NeuralG2P::NeuralG2P(G2PSymbols symbols) : symbols_(std::move(symbols)) {
    for (size_t i = FIRST_REAL_GRAPHEME; i < symbols_.graphemes.size(); ++i)
        grapheme_ids_.emplace(symbols_.graphemes[i], static_cast<int>(i));
}

bool NeuralG2P::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "[NeuralG2P] Warning: Failed to open " << path << std::endl;
        return false;
    }
    char magic[4];
    uint32_t version = 0, count = 0;
    if (!in.read(magic, 4) || std::memcmp(magic, "G2PE", 4) != 0 || !read_u32(in, version) || version != 1 ||
        !read_u32(in, count)) {
        std::cerr << "[NeuralG2P] Warning: " << path << " is not a G2PE v1 file" << std::endl;
        return false;
    }

    std::unordered_map<std::string, Tensor> t;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t name_len = 0, ndim = 0;
        if (!read_u32(in, name_len) || name_len > 256) return false;
        std::string name(name_len, '\0');
        if (!in.read(&name[0], name_len) || !read_u32(in, ndim) || ndim > 4) return false;
        Tensor tensor;
        tensor.shape.resize(ndim);
        size_t elems = 1;
        for (auto& d : tensor.shape) {
            if (!read_u32(in, d)) return false;
            elems *= d;
        }
        tensor.data.resize(elems);
        if (!in.read(reinterpret_cast<char*>(tensor.data.data()), elems * sizeof(float))) return false;
        t[name] = std::move(tensor);
    }

    const char* required[] = {"enc_emb", "enc_w_ih", "enc_w_hh", "enc_b_ih", "enc_b_hh", "dec_emb",
                              "dec_w_ih", "dec_w_hh", "dec_b_ih", "dec_b_hh", "fc_w",     "fc_b"};
    for (const char* name : required) {
        if (!t.count(name) || t[name].shape.empty()) {
            std::cerr << "[NeuralG2P] Warning: missing tensor " << name << " in " << path << std::endl;
            return false;
        }
    }
    const uint32_t emb = t["enc_emb"].shape.back();
    const uint32_t h = t["enc_w_hh"].shape.back();
    const uint32_t g = 3 * h;
    const auto graphemes = static_cast<uint32_t>(symbols_.graphemes.size());
    const auto phonemes = static_cast<uint32_t>(symbols_.phonemes.size());
    const bool shapes_ok =
        has_shape(t["enc_emb"], {graphemes, emb}) && has_shape(t["dec_emb"], {phonemes, emb}) &&
        has_shape(t["enc_w_ih"], {g, emb}) && has_shape(t["dec_w_ih"], {g, emb}) &&
        has_shape(t["enc_w_hh"], {g, h}) && has_shape(t["dec_w_hh"], {g, h}) &&
        has_shape(t["enc_b_ih"], {g}) && has_shape(t["enc_b_hh"], {g}) && has_shape(t["dec_b_ih"], {g}) &&
        has_shape(t["dec_b_hh"], {g}) && has_shape(t["fc_w"], {phonemes, h}) &&
        has_shape(t["fc_b"], {phonemes});
    if (!shapes_ok) {
        std::cerr << "[NeuralG2P] Warning: unexpected tensor shapes in " << path << std::endl;
        return false;
    }

    enc_in_ = project(t["enc_emb"], t["enc_w_ih"], t["enc_b_ih"]);
    dec_in_ = project(t["dec_emb"], t["dec_w_ih"], t["dec_b_ih"]);
    enc_w_hh_ = std::move(t["enc_w_hh"].data);
    enc_b_hh_ = std::move(t["enc_b_hh"].data);
    dec_w_hh_ = std::move(t["dec_w_hh"].data);
    dec_b_hh_ = std::move(t["dec_b_hh"].data);
    fc_w_ = std::move(t["fc_w"].data);
    fc_b_ = std::move(t["fc_b"].data);
    hidden_ = static_cast<int>(h);
    return true;
}

void NeuralG2P::gru_step(const float* gi, const std::vector<float>& w_hh, const std::vector<float>& b_hh,
                         std::vector<float>& h, std::vector<float>& gh) const {
    const int H = hidden_;
    for (int j = 0; j < 3 * H; ++j) {
        const float* wr = &w_hh[static_cast<size_t>(j) * H];
        float acc = b_hh[j];
        for (int k = 0; k < H; ++k) acc += wr[k] * h[k];
        gh[j] = acc;
    }
    // PyTorch GRU gate order: reset, update, new.
    for (int j = 0; j < H; ++j) {
        const float r = sigmoid(gi[j] + gh[j]);
        const float z = sigmoid(gi[H + j] + gh[H + j]);
        const float n = std::tanh(gi[2 * H + j] + r * gh[2 * H + j]);
        h[j] = (1.0f - z) * n + z * h[j];
    }
}

std::vector<std::string> NeuralG2P::predict(const std::string& word) const {
    std::vector<std::string> out;
    if (!loaded()) return out;
    const int H = hidden_;
    const size_t G = static_cast<size_t>(3) * H;
    std::vector<float> h(H, 0.0f), gh(G);

    std::string ch;  // one UTF-8 character; short-string storage, no heap allocation
    for (size_t i = 0; i < word.size(); i += ch.size()) {
        ch.assign(word, i, std::min(utf8_char_length(static_cast<unsigned char>(word[i])), word.size() - i));
        const auto it = grapheme_ids_.find(ch);
        const size_t idx = it == grapheme_ids_.end() ? GRAPHEME_UNK : static_cast<size_t>(it->second);
        gru_step(&enc_in_[idx * G], enc_w_hh_, enc_b_hh_, h, gh);
    }
    gru_step(&enc_in_[GRAPHEME_EOS * G], enc_w_hh_, enc_b_hh_, h, gh);

    int prev = PHONEME_BOS;
    for (int step = 0; step < MAX_DECODE_STEPS; ++step) {
        gru_step(&dec_in_[prev * G], dec_w_hh_, dec_b_hh_, h, gh);
        int best = 0;
        float best_logit = -INFINITY;
        for (int p = 0; p < static_cast<int>(symbols_.phonemes.size()); ++p) {
            const float* wr = &fc_w_[static_cast<size_t>(p) * H];
            float acc = fc_b_[p];
            for (int k = 0; k < H; ++k) acc += wr[k] * h[k];
            if (acc > best_logit) {
                best_logit = acc;
                best = p;
            }
        }
        if (best == PHONEME_EOS) break;
        if (best >= FIRST_REAL_PHONEME) out.push_back(symbols_.phonemes[best]);
        prev = best;
    }
    return out;
}
