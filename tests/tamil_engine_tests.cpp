#include "input_engine/tamil_transliterator.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

int main() {
    const input_engine::TamilTransliterator engine;
    const std::vector<std::pair<std::string, std::string>> cases{
        {"vanakkam", "வணக்கம்"},
        {"amma", "அம்மா"},
        {"tamizh", "தமிழ்"},
        {"nandri", "நன்றி"},
        {"ulagam", "உலகம்"}
    };

    for (const auto& [input, expected] : cases) {
        const auto actual = engine.transliterate(input);
        if (actual != expected) {
            std::cerr << "FAIL: " << input << " expected [" << expected
                      << "] got [" << actual << "]\n";
            return 1;
        }
    }
    std::cout << "Tamil engine smoke tests passed\n";
    return 0;
}
