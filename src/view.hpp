#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// view.hpp – XDG toplevel window (a "view" in compositor terminology)
// ──────────────────────────────────────────────────────────────────────────────

extern "C" {
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
}

struct TXServer;

// ── TXView ─────────────────────────────────────────────────────────────────────
// Represents one mapped XDG toplevel window.
// Lives in TXServer::views as a std::list node.
struct TXView {
    TXServer          *server       = nullptr;
    wlr_xdg_toplevel  *xdg_toplevel = nullptr;
    wlr_scene_tree    *scene_tree   = nullptr;

    // Position on the output layout (in logical coordinates)
    int x = 0;
    int y = 0;

    // Signal listeners (lifetime == TXView lifetime)
    wl_listener map;
    wl_listener unmap;
    wl_listener destroy;
    wl_listener request_move;
    wl_listener request_resize;
    wl_listener request_maximize;
    wl_listener request_fullscreen;
};
