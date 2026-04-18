#include "weasel/lsp/document_store.hpp"
#include "weasel/compiler/boundary.hpp"
#include "weasel/compiler/diagnostic.hpp"
#include "weasel/compiler/transpiler.hpp"
#include <cctype>
#include <cstdio>
#include <ostream>
#include <sstream>
#include <streambuf>

namespace weasel::lsp {

namespace {
class null_streambuf : public std::streambuf {
  protected:
    int overflow(int c) override { return c; }
    std::streamsize xsputn(const char*, std::streamsize n) override { return n; }
};
} // namespace

bool doc_state::position_in_ccx(size_t offset) const {
    for (const auto& s : ccx_spans) {
        if (offset >= s.begin && offset < s.end) return true;
    }
    return false;
}

void document_store::refresh(doc_state& d) {
    d.buffer = weasel::compiler::make_source(d.uri, d.text);
    d.components = weasel::compiler::collect_component_infos(d.text);
    d.ccx_spans.clear();
    d.diagnostics.clear();

    null_streambuf nb;
    std::ostream null_out(&nb);
    weasel::compiler::transpile_options opts;
    opts.on_ccx_span = [&](size_t b, size_t e) {
        d.ccx_spans.push_back({b, e});
    };
    try {
        weasel::compiler::transpile(d.text, null_out, opts);
    } catch (const weasel::compiler::parse_error& e) {
        d.diagnostics.push_back(e.diag);
    } catch (const std::exception& e) {
        weasel::compiler::diagnostic fallback;
        fallback.sev = weasel::compiler::severity::error;
        fallback.message = e.what();
        fallback.code = "weasel.internal";
        d.diagnostics.push_back(std::move(fallback));
    }
}

doc_state& document_store::open_or_update(std::string uri, std::string text, int version) {
    auto& d = docs_[uri];
    d.uri = std::move(uri);
    d.text = std::move(text);
    d.version = version;
    refresh(d);
    return d;
}

void document_store::close(const std::string& uri) {
    docs_.erase(uri);
}

const doc_state* document_store::find(const std::string& uri) const {
    auto it = docs_.find(uri);
    return it == docs_.end() ? nullptr : &it->second;
}

doc_state* document_store::find(const std::string& uri) {
    auto it = docs_.find(uri);
    return it == docs_.end() ? nullptr : &it->second;
}

std::optional<std::string> uri_to_path(std::string_view uri) {
    constexpr std::string_view prefix = "file://";
    if (uri.substr(0, prefix.size()) != prefix) return std::nullopt;
    std::string_view rest = uri.substr(prefix.size());
    // Drop optional host: file:///path — we see an extra '/'.
    std::string out;
    out.reserve(rest.size());
    for (size_t i = 0; i < rest.size(); ++i) {
        char c = rest[i];
        if (c == '%' && i + 2 < rest.size() &&
            std::isxdigit(static_cast<unsigned char>(rest[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(rest[i + 2]))) {
            auto hexv = [](char h) {
                if (h >= '0' && h <= '9') return h - '0';
                if (h >= 'a' && h <= 'f') return 10 + (h - 'a');
                if (h >= 'A' && h <= 'F') return 10 + (h - 'A');
                return 0;
            };
            out.push_back(static_cast<char>((hexv(rest[i + 1]) << 4) | hexv(rest[i + 2])));
            i += 2;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string path_to_uri(std::string_view path) {
    std::ostringstream oss;
    oss << "file://";
    for (char c : path) {
        unsigned char uc = static_cast<unsigned char>(c);
        bool unreserved = (uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') ||
                          (uc >= '0' && uc <= '9') ||
                          uc == '-' || uc == '_' || uc == '.' || uc == '~' ||
                          uc == '/' || uc == ':';
        if (unreserved) {
            oss << c;
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", uc);
            oss << buf;
        }
    }
    return oss.str();
}

lsp_position to_lsp_position(const weasel::compiler::source_buffer& buf, size_t offset) {
    auto p = buf.position_of(offset);
    return {static_cast<int>(p.line) - 1, static_cast<int>(p.column)};
}

size_t offset_from_lsp_position(const weasel::compiler::source_buffer& buf,
                                int line, int character) {
    // line is 0-based in LSP; line_starts is 0-based-indexed and represents 1-based
    // lines: line_starts[0] = start of line 1, line_starts[1] = start of line 2, etc.
    if (line < 0) line = 0;
    if (character < 0) character = 0;
    size_t lidx = static_cast<size_t>(line);
    if (lidx >= buf.line_starts.size()) return buf.text.size();
    size_t start = buf.line_starts[lidx];
    size_t line_end = (lidx + 1 < buf.line_starts.size())
                          ? buf.line_starts[lidx + 1] - 1
                          : buf.text.size();
    size_t off = start + static_cast<size_t>(character);
    return off > line_end ? line_end : off;
}

} // namespace weasel::lsp
