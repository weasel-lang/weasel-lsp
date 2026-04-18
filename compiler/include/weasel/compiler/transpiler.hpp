#pragma once
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace weasel::compiler {

struct transpile_options {
    // Invoked for each CCX region detected in the source, with the byte-offset
    // range [begin, end) of the CCX markup in the input. Optional; may be null.
    std::function<void(size_t begin, size_t end)> on_ccx_span;
};

struct component_info {
    std::string name;
    size_t decl_offset;        // byte offset of the component name in src
    size_t decl_line;          // 1-based
    size_t decl_column;        // 0-based, byte column
    std::string params_raw_text; // text inside the outermost '(' ... ')', not including parens
};

std::unordered_set<std::string> collect_components(std::string_view src);
std::vector<component_info> collect_component_infos(std::string_view src);

void transpile(std::string_view src,
               std::ostream& out,
               const transpile_options& opts = {});

} // namespace weasel::compiler
