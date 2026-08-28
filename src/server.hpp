#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// server.hpp – Central TXServer state: owns every wlroots object
// ──────────────────────────────────────────────────────────────────────────────

#include <list>
#include <string>

extern "C" {
// wlroots uses C99 [static N] array params which GCC/Clang C++ rejects
// Suppress those warnings/errors for these headers only
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"
// Clang also needs this
#ifdef __clang__
#  pragma clang diagnostic ignored "-Wgnu-folding-constant"
#endif
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <xkbcommon/xkbcommon.h>
#pragma GCC diagnostic pop
}


// Forward declarations
struct TXView;
struct TXOutput;
struct TXKeyboard;

// ── Cursor grab modes ──────────────────────────────────────────────────────────
enum class CursorMode {
    Passthrough,   // Default – just forward events
    Move,          // Dragging a window
    Resize,        // Resizing a window
};

// ── Central compositor server ──────────────────────────────────────────────────
struct TXServer {
    // Wayland core
    wl_display               *display   = nullptr;
    wl_event_loop            *event_loop= nullptr;

    // wlroots backend stack
    wlr_backend              *backend   = nullptr;
    wlr_renderer             *renderer  = nullptr;
    wlr_allocator            *allocator = nullptr;

    // Protocols
    wlr_compositor           *compositor= nullptr;
    wlr_subcompositor        *subcompositor = nullptr;
    wlr_data_device_manager  *data_device_mgr = nullptr;
    wlr_xdg_shell            *xdg_shell = nullptr;

    // Output layout + scene
    wlr_output_layout        *output_layout = nullptr;
    wlr_scene                *scene     = nullptr;
    wlr_scene_output_layout  *scene_output_layout = nullptr;

    // Seat (keyboard/pointer focus)
    wlr_seat                 *seat      = nullptr;

    // Cursor
    wlr_cursor               *cursor    = nullptr;
    wlr_xcursor_manager      *cursor_mgr= nullptr;
    CursorMode                cursor_mode= CursorMode::Passthrough;

    // Grab state (for move/resize)
    TXView                   *grabbed_view  = nullptr;
    double                    grab_x        = 0.0;
    double                    grab_y        = 0.0;
    struct { int x, y, width, height; } grab_geobox{};
    uint32_t                  resize_edges  = 0;

    // Tracked objects
    std::list<TXOutput>       outputs;
    std::list<TXView>         views;
    std::list<TXKeyboard>     keyboards;

    // Signal listeners
    wl_listener new_output;
    wl_listener new_xdg_surface;
    wl_listener new_input;
    wl_listener request_cursor;
    wl_listener request_set_selection;
    wl_listener cursor_motion;
    wl_listener cursor_motion_absolute;
    wl_listener cursor_button;
    wl_listener cursor_axis;
    wl_listener cursor_frame;

    // Init / teardown
    bool init();
    void run();
    void destroy();

    // Helpers
    TXView *view_at(double lx, double ly,
                    wlr_surface **surface_out,
                    double *sx_out, double *sy_out);
    void    focus_view(TXView *view, wlr_surface *surface);
    void    process_cursor_motion(uint32_t time_msec);
    void    begin_interactive(TXView *view, CursorMode mode, uint32_t edges);
};
