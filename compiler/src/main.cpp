#include "weasel/compiler/source.hpp"
#include "weasel/compiler/transpiler.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {
void usage() {
    std::cerr << "usage: weaselc <input.weasel> [-o <output.cc>]\n";
}
} // namespace

int main(int argc, char** argv) {
    std::string input;
    std::string output;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) {
            output = argv[++i];
        } else if (a == "--" && i + 1 < argc) {
            input = argv[++i];
        } else if (!a.empty() && a[0] == '-') {
            usage();
            return 2;
        } else if (input.empty()) {
            input = a;
        } else {
            usage();
            return 2;
        }
    }
    if (input.empty()) { usage(); return 2; }
    if (output.empty()) {
        auto dot = input.find_last_of('.');
        output = (dot == std::string::npos) ? input + ".cc" : input.substr(0, dot) + ".cc";
    }

    weasel::compiler::source_buffer buf;
    try {
        buf = weasel::compiler::load_source(input);
    } catch (const std::exception& e) {
        std::cerr << "weaselc: " << e.what() << "\n";
        return 2;
    }

    std::ofstream out(output, std::ios::binary);
    if (!out) {
        std::cerr << "weaselc: cannot write " << output << "\n";
        return 2;
    }
    try {
        weasel::compiler::transpile(buf.text, out);
    } catch (const std::exception& e) {
        std::cerr << "weaselc: " << input << ": " << e.what() << "\n";
        return 1;
    }
    return 0;
}
