#pragma once
#include "weasel/compiler/ccx_parser.hpp"
#include <ostream>

namespace weasel::compiler {

void emit(const ccx_node& n, std::ostream& out);

} // namespace weasel::compiler
