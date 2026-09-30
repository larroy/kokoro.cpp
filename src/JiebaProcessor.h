#pragma once

#include "ZHG2P.h"
#include "PinyinFinder.h"
#include "cppjieba/Jieba.hpp"
#include "Utils.h"
#include <fstream>
#include <stdexcept>

class JiebaProcessor : public TextProcessor {
public:
    JiebaProcessor(const std::string& dict_path, 
                   const std::string& hmm_path, 
                   const std::string& user_dict_path,
                   const std::string& idf_path, 
                   const std::string& stop_word_path,
                   const std::string& pinyin_char_path,
                   const std::string& pinyin_word_path) 
        : jieba(require_file(dict_path), require_file(hmm_path), require_file(user_dict_path),
                require_file(idf_path), require_file(stop_word_path))
    {
        finder = std::make_shared<PinyinFinder>();
        if (!finder->init(pinyin_char_path, pinyin_word_path)) {
            throw std::runtime_error("Failed to load pinyin dictionaries: " + pinyin_char_path + ", " + pinyin_word_path);
        }
    }

    std::vector<std::pair<std::string, std::string>> cut(const std::string& text) override {
        std::vector<std::pair<std::string, std::string>> result;
        std::vector<std::pair<std::string, std::string>> tag_words;
        
        // Use cppjieba Tagging
        jieba.Tag(text, tag_words);
        
        for (const auto& w : tag_words) {
            std::string word = w.first;
            std::string tag = w.second;
            
            // Ensure punctuation is 'x' (jieba might return 'w' for punct)
            if (tag == "w") tag = "x";
            
            // FIX: If tag is 'x' but contains Chinese characters, force it to a valid tag (e.g. 'n')
            // This prevents words like "我要" being tagged as 'x' and skipped by G2P.
            if (tag == "x") {
                bool has_cn = false;
                for (unsigned char c : word) {
                    if (c >= 0xE4 && c <= 0xE9) {
                        has_cn = true; 
                        break;
                    }
                }
                if (has_cn) tag = "n";
            }

            // FIX: a standalone word of only ASCII letters (e.g. "I", "a") is tagged 'x' by
            // cppjieba's special rule and would be skipped by G2P; force it to English.
            if (tag == "x" && !word.empty()) {
                bool all_ascii_letters = true;
                for (const unsigned char c : word) {
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
                        all_ascii_letters = false;
                        break;
                    }
                }
                if (all_ascii_letters) tag = "eng";
            }
            
            result.push_back({word, tag});
        }
        return result;
    }

    std::vector<std::string> word_to_pinyin(const std::string& word) override {
        std::vector<std::string> pinyins;
        if (finder) {
            finder->find_best_pinyin(word, pinyins);
        }
        return pinyins;
    }

private:
    // cppjieba aborts the process on a missing dictionary; fail with an exception first.
    static const std::string& require_file(const std::string& path) {
        if (!std::ifstream(path).is_open()) {
            throw std::runtime_error("Failed to open jieba dictionary: " + path);
        }
        return path;
    }

    cppjieba::Jieba jieba;
    std::shared_ptr<PinyinFinder> finder;
};
