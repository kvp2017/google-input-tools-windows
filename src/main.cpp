#include "input_engine/tamil_transliterator.h"

#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: tamil-ime-demo <roman-text>\n";
        return 0;
    }

    input_engine::TamilTransliterator engine;
    std::cout << engine.transliterate(argv[1]) << '\n';
    return 0;
}
