#pragma once

#include <string>
#include <string_view>

namespace input_engine {

// Prototype transliterator. Input is roman text; output is UTF-8 Tamil text.
// The current dictionary is deliberately small and must be expanded with reviewed tests.
class TamilTransliterator {
public:
    [[nodiscard]] std::string transliterate(std::string_view roman) const;
};

} // namespace input_engine
