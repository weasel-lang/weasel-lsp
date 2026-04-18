#pragma once
#include <iosfwd>
#include <string>
#include <string_view>
#include <unordered_set>

namespace weasel::compiler {

struct transpile_options {};

std::unordered_set<std::string> collect_components(std::string_view src);

void transpile(std::string_view src,
               std::ostream& out,
               const transpile_options& opts = {});

} // namespace weasel::compiler
