#pragma once
#include "weasel/compiler/ccx_parser.hpp"
#include <ostream>

namespace weasel::compiler {

// start_line: 1-based output line we are currently on; 0 disables line-advance.
void emit(const ccx_node& n, std::ostream& out, size_t start_line = 0);

} // namespace weasel::compiler
