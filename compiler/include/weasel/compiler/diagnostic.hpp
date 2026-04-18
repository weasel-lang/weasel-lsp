#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>

namespace weasel::compiler {

enum class severity { error, warning, info, hint };

struct byte_span {
    size_t begin;
    size_t end;
};

struct diagnostic {
    severity sev = severity::error;
    byte_span span = {0, 0};
    std::string message;
    std::string code;  // optional diagnostic code/id
};

// Thrown by the CCX parser when it cannot continue. Carries a structured
// diagnostic so LSP hosts can surface line/column info; the CLI can still
// catch it and print the message.
class parse_error : public std::runtime_error {
  public:
    explicit parse_error(diagnostic d)
        : std::runtime_error(d.message), diag(std::move(d)) {}
    diagnostic diag;
};

} // namespace weasel::compiler
