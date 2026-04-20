#include "weasel/compiler/emitter.hpp"
#include <cstdio>
#include <string>

namespace weasel::compiler {
namespace {

std::string cpp_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        unsigned char uc = static_cast<unsigned char>(c);
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
                // UTF-8 bytes pass through as-is (byte-for-byte identity).
                // Control characters that have no named escape get \xNN so they
                // don't silently corrupt the generated C++ string literal.
                if (uc < 0x20 || uc == 0x7f) {
                    char buf[5];
                    std::snprintf(buf, sizeof(buf), "\\x%02X", uc);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

struct emit_state {
    std::ostream& out;
    size_t cur_line;  // 1-based; 0 = line-advance disabled

    // Emit newlines until we reach target_line. No-op if disabled or already there.
    void advance_to(size_t target_line) {
        if (cur_line == 0 || target_line == 0 || target_line <= cur_line)
            return;
        while (cur_line < target_line) {
            out << '\n';
            ++cur_line;
        }
    }
};

void emit_node(const ccx_node& n, emit_state& st);

void emit_body(const std::vector<ccx_node>& body, emit_state& st) {
    if (body.empty()) {
        st.out << "weasel::node{}";
    } else if (body.size() == 1) {
        st.advance_to(body[0].source_line);
        emit_node(body[0], st);
    } else {
        st.out << "weasel::fragment({";
        for (size_t i = 0; i < body.size(); ++i) {
            if (i > 0)
                st.out << ", ";
            st.advance_to(body[i].source_line);
            emit_node(body[i], st);
        }
        st.out << "})";
    }
}

void emit_element(const ccx_node& n, emit_state& st) {
    if (n.is_component) {
        st.out << n.tag_name << "({";
        for (size_t i = 0; i < n.attrs.size(); ++i) {
            const auto& a = n.attrs[i];
            if (i > 0)
                st.out << ", ";
            st.out << "." << a.name << " = ";
            if (!a.has_value)
                st.out << "true";
            else if (a.is_literal)
                st.out << a.value_cpp;
            else
                st.out << "(" << a.value_cpp << ")";
        }
        st.out << "})";
        return;
    }

    st.out << "weasel::tag(\"" << n.tag_name << "\"";
    if (!n.attrs.empty() || !n.children.empty()) {
        st.out << ", {";
        for (size_t i = 0; i < n.attrs.size(); ++i) {
            const auto& a = n.attrs[i];
            if (i > 0)
                st.out << ", ";
            st.out << "{\"" << a.name << "\", ";
            if (!a.has_value)
                st.out << "\"\"";
            else if (a.is_literal)
                st.out << a.value_cpp;
            else
                st.out << "(" << a.value_cpp << ")";
            st.out << "}";
        }
        st.out << "}";
    }
    if (!n.children.empty()) {
        st.out << ", {";
        for (size_t i = 0; i < n.children.size(); ++i) {
            if (i > 0)
                st.out << ", ";
            st.advance_to(n.children[i].source_line);
            emit_node(n.children[i], st);
        }
        st.out << "}";
    }
    st.out << ")";
}

void emit_if(const ccx_node& n, emit_state& st) {
    st.out << "[&]() -> weasel::node {";
    bool first = true;
    for (const auto& br : n.branches) {
        st.advance_to(br.source_line);
        if (br.is_else) {
            st.out << "else { return ";
            emit_body(br.body, st);
            st.out << "; } ";
        } else {
            st.out << (first ? "if (" : "else if (") << br.cond_cpp << ") { return ";
            emit_body(br.body, st);
            st.out << "; } ";
            first = false;
        }
    }
    st.out << "return weasel::node{}; }()";
}

void emit_for(const ccx_node& n, emit_state& st) {
    st.out << "[&]() -> weasel::node { weasel::node_list weasel_nodes_; for (" << n.head_cpp << ") { weasel_nodes_.push_back(";
    emit_body(n.children, st);
    st.out << "); } return weasel::fragment(std::move(weasel_nodes_)); }()";
}

void emit_while(const ccx_node& n, emit_state& st) {
    st.out << "[&]() -> weasel::node { weasel::node_list weasel_nodes_; while (" << n.head_cpp << ") { weasel_nodes_.push_back(";
    emit_body(n.children, st);
    st.out << "); } return weasel::fragment(std::move(weasel_nodes_)); }()";
}

void emit_node(const ccx_node& n, emit_state& st) {
    switch (n.k) {
        case ccx_node::kind::element:
            emit_element(n, st);
            break;
        case ccx_node::kind::text:
            st.out << "weasel::text(\"" << cpp_escape(n.text_content) << "\")";
            break;
        case ccx_node::kind::expr_child:
            st.out << "weasel::text(" << n.expr_text << ")";
            break;
        case ccx_node::kind::if_chain:
            emit_if(n, st);
            break;
        case ccx_node::kind::for_loop:
            emit_for(n, st);
            break;
        case ccx_node::kind::while_loop:
            emit_while(n, st);
            break;
    }
}

}  // namespace

void emit(const ccx_node& n, std::ostream& out, size_t start_line) {
    emit_state st{out, start_line};
    emit_node(n, st);
}

}  // namespace weasel::compiler
