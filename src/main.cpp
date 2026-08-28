// ──────────────────────────────────────────────────────────────────────────────
// main.cpp – TerminateX compositor entrypoint (compiled as C++)
//
// The ONLY C++ translation unit. Calls into the C compositor core via
// tx_server.h. Includes wlr/util/log.h directly (it has no [static N] params).
// ──────────────────────────────────────────────────────────────────────────────

#define WLR_USE_UNSTABLE
#include "tx_server.h"   // Pure C header — safe to include in C++

// wlr/util/log.h is safe in C++ — no [static N] array params.
extern "C" {
#include <wlr/util/log.h>
}

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

int main(int argc, char *argv[]) {
    wlr_log_init(WLR_INFO, nullptr);
    wlr_log(WLR_INFO, "TerminateX starting up...");

    // Optional startup command: ./terminatex foot
    const char *startup_cmd = (argc >= 2) ? argv[1] : nullptr;

    TXServer server{};
    if (!tx_server_init(&server)) {
        wlr_log(WLR_ERROR, "Failed to initialise compositor. Aborting.");
        return EXIT_FAILURE;
    }

    if (startup_cmd) {
        wlr_log(WLR_INFO, "Launching startup command: %s", startup_cmd);
        if (fork() == 0) {
            unsetenv("LD_PRELOAD");
            execl("/bin/sh", "/bin/sh", "-c", startup_cmd, nullptr);
            _exit(1);
        }
    }

    wlr_log(WLR_INFO, "Entering event loop...");
    tx_server_run(&server);

    tx_server_destroy(&server);
    wlr_log(WLR_INFO, "TerminateX shut down cleanly.");
    return EXIT_SUCCESS;
}
