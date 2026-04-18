#pragma once
#include "weasel/compiler/jsx_parser.hpp"
#include <ostream>

namespace weasel::compiler {

void emit(const jsx_node& n, std::ostream& out);

} // namespace weasel::compiler
