#include "weasel/lsp/features.hpp"

namespace weasel::lsp {

namespace {
int to_lsp_severity(weasel::compiler::severity s) {
    using S = weasel::compiler::severity;
    switch (s) {
    case S::error:   return 1;
    case S::warning: return 2;
    case S::info:    return 3;
    case S::hint:    return 4;
    }
    return 1;
}
} // namespace

json build_diagnostics_payload(const doc_state& d) {
    json arr = json::array();
    for (const auto& diag : d.diagnostics) {
        auto start = to_lsp_position(d.buffer, diag.span.begin);
        auto end = to_lsp_position(d.buffer, diag.span.end == diag.span.begin
                                                 ? diag.span.begin + 1
                                                 : diag.span.end);
        json entry = {
            {"range", {
                {"start", {{"line", start.line}, {"character", start.character}}},
                {"end",   {{"line", end.line},   {"character", end.character}}},
            }},
            {"severity", to_lsp_severity(diag.sev)},
            {"source", "weasel"},
            {"message", diag.message},
        };
        if (!diag.code.empty()) entry["code"] = diag.code;
        arr.push_back(std::move(entry));
    }
    return arr;
}

} // namespace weasel::lsp
