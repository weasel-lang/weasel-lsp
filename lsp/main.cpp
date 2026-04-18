#include "weasel/lsp/server.hpp"
#include <iostream>

int main() {
    // LSP traffic uses stdout; stderr is for logs. Ensure stdout is unbuffered
    // enough for framing writes to flush promptly (write_message calls flush()).
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    weasel::lsp::server srv(std::cin, std::cout);
    return srv.run();
}
