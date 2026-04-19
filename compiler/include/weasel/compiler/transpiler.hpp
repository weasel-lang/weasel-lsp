#pragma once
#include "weasel/compiler/diagnostic.hpp"
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

enum class span_kind {
    cpp_passthrough,
    ccx_region,
};

// Describes a contiguous run of generated .cc lines and the .weasel lines they
// correspond to. cc line ranges are disjoint and contiguous across the vector;
// weasel line ranges may share a boundary line between adjacent spans if the
// source line contained both C++ and CCX content. All lines 1-based inclusive.
struct line_span {
    size_t weasel_line_begin;
    size_t weasel_line_end;
    size_t cc_line_begin;
    size_t cc_line_end;
    span_kind kind;
};

struct transpile_result {
    bool ok = true;
    std::vector<diagnostic> diagnostics;
    std::vector<line_span> line_map;
    std::vector<component_info> components;
};

std::unordered_set<std::string> collect_components(std::string_view src);
std::vector<component_info> collect_component_infos(std::string_view src);

// CLI-style transpile: writes to `out`, throws parse_error on failure.
void transpile(std::string_view src,
               std::ostream& out,
               const transpile_options& opts = {});

// LSP-style transpile: never throws. Returns a structured result with
// diagnostics, a line map (for remapping clangd results back to the .weasel),
// and component metadata.
transpile_result transpile_with_map(std::string_view src,
                                    std::ostream& out,
                                    const transpile_options& opts = {});

// Given a line_map sorted by cc_line_begin and a 1-based cc line number,
// return the weasel line it corresponds to plus the span it belongs to.
// For cpp_passthrough, the line offset within the span is preserved.
// For ccx_region, the span's weasel_line_begin is returned.
// If no span covers the line (past EOF), returns {0, nullptr}.
struct cc_to_weasel_result {
    size_t weasel_line;          // 1-based; 0 if not found
    const line_span* span;       // nullptr if not found
};
cc_to_weasel_result cc_line_to_weasel(const std::vector<line_span>& map,
                                      size_t cc_line);

} // namespace weasel::compiler
