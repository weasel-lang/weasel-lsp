#pragma once
#include <cstddef>
#include <string_view>
#include <vector>

namespace weasel::compiler {

struct ccx_span {
    size_t begin;  // byte offset of '<' that opens the CCX region
    size_t end;    // byte offset one past the last char consumed by CCX parsing
};

// Walk `src` and return the byte ranges of all CCX regions.
// Internally runs the transpiler in scan mode (discards output).
std::vector<ccx_span> find_ccx_spans(std::string_view src);

// Returns true if `offset` is inside any CCX region in `src`.
// Equivalent to find_ccx_spans(src) + linear search; for a single query this is
// efficient enough, but callers that query repeatedly on the same source should
// cache find_ccx_spans() themselves.
bool is_position_in_ccx(std::string_view src, size_t offset);

}  // namespace weasel::compiler
