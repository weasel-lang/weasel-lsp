#include "weasel/lsp/server.hpp"
#include <csignal>
#include <iostream>
#include <unistd.h>

// Close stdin on SIGTERM/SIGHUP so read_message() returns EOF and the server
// loop exits cleanly, allowing clangd_proxy::shutdown() to run.
static void handle_signal(int) { ::close(STDIN_FILENO); }

int main() {
    // LSP traffic uses stdout; stderr is for logs. Ensure stdout is unbuffered
    // enough for framing writes to flush promptly (write_message calls flush()).
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    struct sigaction sa{};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    ::sigaction(SIGTERM, &sa, nullptr);
    ::sigaction(SIGHUP,  &sa, nullptr);

    weasel::lsp::server srv(std::cin, std::cout);
    return srv.run();
}
