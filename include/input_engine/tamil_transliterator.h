#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace input_engine {
class TamilTransliterator {
public:
    [[nodiscard]] std::vector<std::string> candidates(std::string_view roman) const;
    [[nodiscard]] std::string transliterate(std::string_view roman) const;
};
}
