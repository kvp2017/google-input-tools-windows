#include "input_engine/tamil_transliterator.h"

#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>

namespace input_engine {
namespace {

const std::unordered_map<std::string, std::string>& word_map() {
    static const std::unordered_map<std::string, std::string> words{
        {"vanakkam", "வணக்கம்"},
        {"amma", "அம்மா"},
        {"tamizh", "தமிழ்"},
        {"tamil", "தமிழ்"},
        {"nandri", "நன்றி"},
        {"ulagam", "உலகம்"},
        {"anbu", "அன்பு"},
        {"nanban", "நண்பன்"},
        {"palli", "பள்ளி"},
        {"kalvi", "கல்வி"},
        {"appa", "அப்பா"}
    };
    return words;
}

// Small greedy roman-token fallback. Word overrides above take precedence.
// This is an experimental mapping, not a complete transliteration specification.
const std::unordered_map<std::string, std::string>& tokens() {
    static const std::unordered_map<std::string, std::string> map{
        {"ng", "ங்"}, {"nj", "ஞ்"}, {"zh", "ழ்"}, {"sh", "ஷ்"},
        {"th", "த்"}, {"dh", "த்"}, {"ch", "ச்"}, {"ny", "ஞ்"},
        {"aa", "ஆ"}, {"ii", "ஈ"}, {"ee", "ஈ"}, {"uu", "ஊ"},
        {"oo", "ஊ"}, {"ai", "ஐ"}, {"au", "ஔ"},
        {"k", "க்"}, {"g", "க்"}, {"c", "ச்"}, {"s", "ஸ்"},
        {"j", "ஜ்"}, {"t", "ட்"}, {"d", "ட்"}, {"n", "ன்"},
        {"p", "ப்"}, {"b", "ப்"}, {"m", "ம்"}, {"y", "ய்"},
        {"r", "ர்"}, {"l", "ல்"}, {"v", "வ்"}, {"w", "வ்"},
        {"h", "ஹ்"}, {"f", "ஃப்"},
        {"a", "அ"}, {"i", "இ"}, {"u", "உ"}, {"e", "எ"},
        {"o", "ஒ"}
    };
    return map;
}

std::string lower_ascii(std::string_view input) {
    std::string result(input);
    for (char& ch : result) {
        const auto c = static_cast<unsigned char>(ch);
        if (c < 128) ch = static_cast<char>(std::tolower(c));
    }
    return result;
}

} // namespace

std::string TamilTransliterator::transliterate(std::string_view roman) const {
    const std::string input = lower_ascii(roman);
    if (const auto it = word_map().find(input); it != word_map().end()) {
        return it->second;
    }

    std::string output;
    const auto& table = tokens();
    std::size_t i = 0;
    while (i < input.size()) {
        const unsigned char current = static_cast<unsigned char>(input[i]);
        if (current >= 128 || std::isspace(current) || std::ispunct(current)) {
            output.push_back(input[i++]);
            continue;
        }

        bool matched = false;
        for (std::size_t length = 3; length >= 1; --length) {
            if (i + length <= input.size()) {
                const auto key = input.substr(i, length);
                if (const auto it = table.find(key); it != table.end()) {
                    output += it->second;
                    i += length;
                    matched = true;
                    break;
                }
            }
            if (length == 1) break;
        }
        if (!matched) output.push_back(input[i++]);
    }
    return output;
}

} // namespace input_engine
