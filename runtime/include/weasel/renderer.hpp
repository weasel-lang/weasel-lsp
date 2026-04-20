#pragma once
#include "weasel/node.hpp"
#include <iosfwd>
#include <string>

namespace weasel {

void render(const node& n, std::ostream& out) noexcept;
std::string render_to_string(const node& n) noexcept;

} // namespace weasel
