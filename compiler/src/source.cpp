#include "weasel/compiler/source.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace weasel::compiler {

namespace {
void strip_bom(std::string& s) {
    if (s.size() >= 3 &&
        static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF) {
        s.erase(0, 3);
    }
}

std::vector<size_t> compute_line_starts(std::string_view s) {
    std::vector<size_t> starts = {0};
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') starts.push_back(i + 1);
    }
    return starts;
}
} // namespace

source_buffer::source_buffer(std::string fname, std::string src)
    : filename(std::move(fname)), text(std::move(src)) {
    strip_bom(text);
    line_starts = compute_line_starts(text);
}

size_t source_buffer::line_of(size_t offset) const {
    auto it = std::upper_bound(line_starts.begin(), line_starts.end(), offset);
    return static_cast<size_t>(it - line_starts.begin());
}

source_buffer load_source(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open file: " + filename);
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    return source_buffer{filename, oss.str()};
}

source_buffer make_source(std::string filename, std::string text) {
    return source_buffer{std::move(filename), std::move(text)};
}

} // namespace weasel::compiler
