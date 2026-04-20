#include "weasel/lsp/jsonrpc.hpp"
#include <istream>
#include <ostream>
#include <string>

namespace weasel::lsp {

std::optional<json> read_message(std::istream& in) {
    std::size_t content_length = 0;
    bool got_length = false;
    std::string line;

    // Read headers until blank line.
    while (true) {
        line.clear();
        char c;
        while (in.get(c)) {
            if (c == '\r') {
                if (in.peek() == '\n')
                    in.get(c);
                break;
            }
            if (c == '\n')
                break;
            line.push_back(c);
        }
        if (!in && line.empty())
            return std::nullopt;
        if (line.empty())
            break;  // end of headers

        // Parse "Header: value"
        auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        // Trim leading whitespace from value
        size_t vs = value.find_first_not_of(" \t");
        if (vs != std::string::npos)
            value = value.substr(vs);
        // Lowercase-compare header name
        std::string lname;
        lname.reserve(name.size());
        for (char cc : name)
            lname.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(cc))));
        if (lname == "content-length") {
            try {
                content_length = std::stoul(value);
                got_length = true;
            } catch (...) {
                return std::nullopt;
            }
        }
    }

    if (!got_length)
        return std::nullopt;
    // Reject pathological sizes before allocating.
    constexpr std::size_t max_message_size = 64u * 1024 * 1024;  // 64 MiB
    if (content_length > max_message_size)
        return std::nullopt;

    std::string body(content_length, '\0');
    std::size_t got = 0;
    while (got < content_length) {
        in.read(body.data() + got, static_cast<std::streamsize>(content_length - got));
        if (in.gcount() <= 0)
            return std::nullopt;
        got += static_cast<std::size_t>(in.gcount());
        if (!in && got < content_length)
            return std::nullopt;
    }

    try {
        return json::parse(body);
    } catch (...) {
        return std::nullopt;
    }
}

void write_message(std::ostream& out, const json& msg) {
    std::string body = msg.dump();
    out << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    out.flush();
}

json make_response(const json& id, json result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

json make_error_response(const json& id, int code, std::string_view message) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", std::string(message)}}}};
}

json make_notification(std::string_view method, json params) {
    return {{"jsonrpc", "2.0"}, {"method", std::string(method)}, {"params", std::move(params)}};
}

}  // namespace weasel::lsp
