#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace weasel::compiler {

struct source_buffer {
    std::string filename;
    std::string text;
    std::vector<size_t> line_starts;

    source_buffer() = default;
    source_buffer(std::string fname, std::string src);

    size_t line_of(size_t offset) const;
};

source_buffer load_source(const std::string& filename);
source_buffer make_source(std::string filename, std::string text);

} // namespace weasel::compiler
