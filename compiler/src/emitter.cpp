#include "weasel/compiler/emitter.hpp"
#include <string>

namespace weasel::compiler {
namespace {

std::string cpp_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
        }
    }
    return out;
}

void emit_body(const std::vector<ccx_node> &body, std::ostream &out) {
    if (body.empty()) {
        out << "weasel::node{}";
    } else if (body.size() == 1) {
        emit(body[0], out);
    } else {
        out << "weasel::fragment({";
        for (size_t i = 0; i < body.size(); ++i) {
            if (i > 0)
                out << ", ";
            emit(body[i], out);
        }
        out << "})";
    }
}

void emit_element(const ccx_node &n, std::ostream &out) {
    if (n.is_component) {
        out << n.tag_name << "({";
        for (size_t i = 0; i < n.attrs.size(); ++i) {
            const auto &a = n.attrs[i];
            if (i > 0)
                out << ", ";
            out << "." << a.name << " = ";
            if (!a.has_value) {
                out << "true";
            } else if (a.is_literal) {
                out << a.value_cpp;
            } else {
                out << "(" << a.value_cpp << ")";
            }
        }
        out << "})";
        return;
    }

    out << "weasel::tag(\"" << n.tag_name << "\"";
    if (!n.attrs.empty() || !n.children.empty()) {
        out << ", {";
        for (size_t i = 0; i < n.attrs.size(); ++i) {
            const auto &a = n.attrs[i];
            if (i > 0)
                out << ", ";
            out << "{\"" << a.name << "\", ";
            if (!a.has_value) {
                out << "\"\"";
            } else if (a.is_literal) {
                out << a.value_cpp;
            } else {
                out << "(" << a.value_cpp << ")";
            }
            out << "}";
        }
        out << "}";
    }
    if (!n.children.empty()) {
        out << ", {";
        for (size_t i = 0; i < n.children.size(); ++i) {
            if (i > 0)
                out << ", ";
            emit(n.children[i], out);
        }
        out << "}";
    }
    out << ")";
}

void emit_if(const ccx_node &n, std::ostream &out) {
    out << "[&]() -> weasel::node { ";
    bool first = true;
    for (const auto &br : n.branches) {
        if (br.is_else) {
            out << "else { return ";
            emit_body(br.body, out);
            out << "; } ";
        } else {
            out << (first ? "if (" : "else if (") << br.cond_cpp << ") { return ";
            emit_body(br.body, out);
            out << "; } ";
            first = false;
        }
    }
    out << "return weasel::node{}; }()";
}

void emit_for(const ccx_node &n, std::ostream &out) {
    out << "[&]() -> weasel::node { weasel::node_list __w; for (" << n.head_cpp << ") { __w.push_back(";
    emit_body(n.children, out);
    out << "); } return weasel::fragment(std::move(__w)); }()";
}

void emit_while(const ccx_node &n, std::ostream &out) {
    out << "[&]() -> weasel::node { weasel::node_list __w; while (" << n.head_cpp << ") { __w.push_back(";
    emit_body(n.children, out);
    out << "); } return weasel::fragment(std::move(__w)); }()";
}

} // namespace

void emit(const ccx_node &n, std::ostream &out) {
    switch (n.k) {
    case ccx_node::kind::element:
        emit_element(n, out);
        break;
    case ccx_node::kind::text:
        out << "weasel::text(\"" << cpp_escape(n.text_content) << "\")";
        break;
    case ccx_node::kind::expr_child:
        out << "weasel::text(" << n.expr_text << ")";
        break;
    case ccx_node::kind::if_chain:
        emit_if(n, out);
        break;
    case ccx_node::kind::for_loop:
        emit_for(n, out);
        break;
    case ccx_node::kind::while_loop:
        emit_while(n, out);
        break;
    }
}

} // namespace weasel::compiler
