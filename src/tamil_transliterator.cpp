#include "input_engine/tamil_transliterator.h"

#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace input_engine {
namespace {

struct Vowel {
    const char* roman;
    const char* independent;
    const char* sign;
};

const std::vector<Vowel>& vowels() {
    static const std::vector<Vowel> values{
        {"aa", "ஆ", "ா"}, {"ii", "ஈ", "ீ"}, {"ee", "ஏ", "ே"},
        {"uu", "ஊ", "ூ"}, {"oo", "ஓ", "ோ"}, {"ai", "ஐ", "ை"},
        {"au", "ஔ", "ௌ"}, {"a", "அ", ""}, {"i", "இ", "ி"},
        {"u", "உ", "ு"}, {"e", "எ", "ெ"}, {"o", "ஒ", "ொ"}
    };
    return values;
}

const std::unordered_map<std::string, std::string>& word_candidates_source() {
    static const std::unordered_map<std::string, std::string> words{
        {"vanakkam", "வணக்கம்"}, {"amma", "அம்மா"},
        {"tamizh", "தமிழ்"}, {"tamil", "தமிழ்"},
        {"nandri", "நன்றி"}, {"ulagam", "உலகம்"},
        {"anbu", "அன்பு"}, {"nanban", "நண்பன்"},
        {"palli", "பள்ளி"}, {"kalvi", "கல்வி"},
        {"appa", "அப்பா"}
    };
    return words;
}

const std::unordered_map<std::string, std::string>& consonants() {
    static const std::unordered_map<std::string, std::string> values{
        {"ng", "ங"}, {"nj", "ஞ"}, {"zh", "ழ"}, {"sh", "ஷ"},
        {"th", "த"}, {"dh", "த"}, {"ch", "ச"}, {"ny", "ஞ"},
        {"ll", "ள"}, {"rr", "ற"}, {"nn", "ன"},
        {"k", "க"}, {"g", "க"}, {"c", "ச"}, {"s", "ச"},
        {"j", "ஜ"}, {"t", "ட"}, {"d", "ட"}, {"n", "ந"},
        {"p", "ப"}, {"b", "ப"}, {"m", "ம"}, {"y", "ய"},
        {"r", "ர"}, {"l", "ல"}, {"v", "வ"}, {"w", "வ"},
        {"h", "ஹ"}, {"f", "ஃப"}
    };
    return values;
}

std::string lower_ascii(std::string_view value) {
    std::string result(value);
    for (char& ch : result) {
        const auto c = static_cast<unsigned char>(ch);
        if (c < 128) ch = static_cast<char>(std::tolower(c));
    }
    return result;
}

bool match_vowel(const std::string& input, std::size_t offset,
                 const Vowel** matched) {
    for (const auto& vowel : vowels()) {
        const std::string key(vowel.roman);
        if (input.compare(offset, key.size(), key) == 0) {
            *matched = &vowel;
            return true;
        }
    }
    return false;
}

bool match_consonant(const std::string& input, std::size_t offset,
                     std::string* base, std::size_t* length) {
    // Longest spelling first so digraphs such as "zh" and "ng" win over
    // their single-letter components.
    for (std::size_t count = 3; count >= 1; --count) {
        if (offset + count <= input.size()) {
            const auto it = consonants().find(input.substr(offset, count));
            if (it != consonants().end()) {
                *base = it->second;
                *length = count;
                return true;
            }
        }
        if (count == 1) break;
    }
    return false;
}

std::string fallback(const std::string& input) {
    std::string output;
    std::size_t i = 0;
    while (i < input.size()) {
        const unsigned char c = static_cast<unsigned char>(input[i]);
        if (c >= 128 || std::isspace(c) || std::ispunct(c)) {
            output.push_back(input[i++]);
            continue;
        }

        std::string consonant;
        std::size_t consonant_length = 0;
        if (match_consonant(input, i, &consonant, &consonant_length)) {
            i += consonant_length;
            const Vowel* vowel = nullptr;
            if (i < input.size() && match_vowel(input, i, &vowel)) {
                output += consonant;
                output += vowel->sign;
                i += std::string(vowel->roman).size();
            } else {
                output += consonant;
                output += "்";
            }
            continue;
        }

        const Vowel* vowel = nullptr;
        if (match_vowel(input, i, &vowel)) {
            output += vowel->independent;
            i += std::string(vowel->roman).size();
            continue;
        }

        output.push_back(input[i++]);
    }
    return output;
}

} // namespace

std::vector<std::string> TamilTransliterator::candidates(std::string_view roman) const {
    const std::string input = lower_ascii(roman);
    if (input == "kalam") return {"காலம்", "களம்"};
    if (input == "palli") return {"பள்ளி", "பல்லி"};
    if (input == "malai") return {"மலை", "மாலை"};
    if (input == "kural") return {"குறள்", "குரல்"};
    if (input == "kodi") return {"கொடி", "கோடி"};
    if (const auto it = word_candidates_source().find(input);
        it != word_candidates_source().end()) {
        return {it->second};
    }
    return {fallback(input)};
}

std::string TamilTransliterator::transliterate(std::string_view roman) const {
    const auto values = candidates(roman);
    return values.empty() ? std::string(roman) : values.front();
}

} // namespace input_engine
