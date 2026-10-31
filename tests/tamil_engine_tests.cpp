#include "input_engine/tamil_transliterator.h"
#include <iostream>
#include <string>
#include <utility>
#include <vector>
int main() {
    const input_engine::TamilTransliterator engine;
    const std::vector<std::pair<std::string, std::string>> cases{
        {"vanakkam", "வணக்கம்"}, {"amma", "அம்மா"}, {"tamizh", "தமிழ்"},
        {"nandri", "நன்றி"}, {"ulagam", "உலகம்"}, {"ANBU", "அன்பு"}
    };
    for (const auto& [input, expected] : cases) {
        const auto actual = engine.transliterate(input);
        if (actual != expected) {
            std::cerr << "FAIL: " << input << " expected [" << expected
                      << "] got [" << actual << "]\n";
            return 1;
        }
    }
    if (engine.transliterate("ka") != "க") return 4;
    if (engine.transliterate("ki") != "கி") return 5;
    if (engine.transliterate("kavi") != "கவி") return 6;
    const auto kalam = engine.candidates("kalam");
    if (kalam.size() != 2 || kalam[0] != "காலம்" || kalam[1] != "களம்") return 2;
    const auto palli = engine.candidates("palli");
    if (palli.size() != 2 || palli[0] != "பள்ளி" || palli[1] != "பல்லி") return 3;
    std::cout << "Tamil transliteration and candidate tests passed\n";
    return 0;
}
