#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// keybinds.hpp – Compile-time keybinding table
// ──────────────────────────────────────────────────────────────────────────────

#include "view.hpp"   // Must be before server.hpp to get complete TXView type

#include <xkbcommon/xkbcommon.h>
#include <functional>
#include <array>
#include <cstdint>
#include <unistd.h>    // fork, execlp, _exit

struct TXServer; // forward decl

// Each keybind: {modifier mask, keysym} → action functor
struct Keybind {
    uint32_t                 mods;   // e.g. WLR_MODIFIER_LOGO
    xkb_keysym_t             sym;
    std::function<void(TXServer&)> action;
};

// WLR_MODIFIER_LOGO = Super/Windows key
static constexpr uint32_t MOD_SUPER = 0x40; // WLR_MODIFIER_LOGO value

// ──────────────────────────────────────────────────────────────────────────────
// Keybinding table – add / remove entries here freely
// ──────────────────────────────────────────────────────────────────────────────
inline std::vector<Keybind> make_keybinds() {
    return {
        // Super + Enter  → launch terminal (foot, then fallback to xterm)
        {
            MOD_SUPER,
            XKB_KEY_Return,
            [](TXServer&) {
                if (fork() == 0) {
                    const char *terms[] = { "foot", "alacritty", "kitty", "xterm", nullptr };
                    for (const char **t = terms; *t; ++t) {
                        execlp(*t, *t, nullptr);
                    }
                    _exit(1);
                }
            }
        },
        // Super + Q  → close focused view (sends xdg_toplevel close request)
        {
            MOD_SUPER,
            XKB_KEY_q,
            [](TXServer &srv) {
                if (srv.views.empty()) return;
                auto &view = srv.views.front();
                if (view.xdg_toplevel)
                    wlr_xdg_toplevel_send_close(view.xdg_toplevel);
            }
        },
        // Super + F  → toggle fullscreen on focused view
        {
            MOD_SUPER,
            XKB_KEY_f,
            [](TXServer &srv) {
                if (srv.views.empty()) return;
                auto &view = srv.views.front();
                if (!view.xdg_toplevel) return;
                bool current = view.xdg_toplevel->current.fullscreen;
                wlr_xdg_toplevel_set_fullscreen(view.xdg_toplevel, !current);
            }
        },
        // Super + Escape  → quit the compositor
        {
            MOD_SUPER,
            XKB_KEY_Escape,
            [](TXServer &srv) {
                wl_display_terminate(srv.display);
            }
        },
    };
}
