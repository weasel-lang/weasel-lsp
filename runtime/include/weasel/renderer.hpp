#pragma once
#include <iosfwd>
#include <string>
#include "weasel/node.hpp"

namespace weasel {

void render(const node& n, std::ostream& out) noexcept;
std::string render_to_string(const node& n) noexcept;

}  // namespace weasel
