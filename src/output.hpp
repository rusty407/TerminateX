#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// output.hpp – Physical/virtual monitor output
// ──────────────────────────────────────────────────────────────────────────────

extern "C" {
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
}

struct TXServer;

// ── TXOutput ──────────────────────────────────────────────────────────────────
// Represents one physical monitor.  Created when wlr_backend announces a new
// output; destroyed on unplug.
struct TXOutput {
    TXServer        *server = nullptr;
    wlr_output      *output = nullptr;
    wlr_scene_output *scene_output = nullptr;

    // Signal listeners
    wl_listener frame;
    wl_listener request_state;
    wl_listener destroy;
};
