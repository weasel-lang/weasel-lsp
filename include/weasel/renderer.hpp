#pragma once
#include "weasel/node.hpp"
#include <iosfwd>
#include <string>

namespace weasel {

void render(const node& n, std::ostream& out);
std::string render_to_string(const node& n);

} // namespace weasel
