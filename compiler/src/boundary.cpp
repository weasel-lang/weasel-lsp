#include "weasel/compiler/boundary.hpp"
#include "weasel/compiler/transpiler.hpp"
#include <ostream>
#include <streambuf>

namespace weasel::compiler {

namespace {
// A streambuf that discards everything written to it.
class null_streambuf : public std::streambuf {
  protected:
    int overflow(int c) override { return c; }
    std::streamsize xsputn(const char *, std::streamsize n) override { return n; }
};
} // namespace

std::vector<ccx_span> find_ccx_spans(std::string_view src) {
    std::vector<ccx_span> out;
    null_streambuf nb;
    std::ostream null_out(&nb);
    transpile_options opts;
    opts.on_ccx_span = [&](size_t b, size_t e) { out.push_back({b, e}); };
    transpile(src, null_out, opts);
    return out;
}

bool is_position_in_ccx(std::string_view src, size_t offset) {
    for (const auto &s : find_ccx_spans(src)) {
        if (offset >= s.begin && offset < s.end) return true;
    }
    return false;
}

} // namespace weasel::compiler
