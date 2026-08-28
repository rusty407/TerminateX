#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// input.hpp – Keyboard and pointer device management
// ──────────────────────────────────────────────────────────────────────────────

#include <vector>

extern "C" {
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <xkbcommon/xkbcommon.h>
}

struct TXServer;

// ── TXKeyboard ────────────────────────────────────────────────────────────────
// Wraps one keyboard device.  Multiple keyboards are supported simultaneously.
struct TXKeyboard {
    TXServer      *server   = nullptr;
    wlr_keyboard  *keyboard = nullptr;

    wl_listener key;
    wl_listener modifiers;
    wl_listener destroy;
};
