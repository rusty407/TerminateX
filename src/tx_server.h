/*
 * tx_server.h  –  Shared C struct definitions for TerminateX compositor.
 *
 * This header is included by BOTH C and C++ translation units.
 * It must NOT include any wlroots headers (to avoid [static N] in C++).
 * Wlroots types are forward-declared as opaque structs.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*
 * wayland-server-core.h is safe to include in both C and C++ —
 * it does NOT use any [static N] array parameter syntax.
 * We need it for struct wl_listener and struct wl_display.
 */
#include <wayland-server-core.h>

/*
 * pixman-1 for wlr_box — also safe in C++.
 */
#include <pixman.h>

/*
 * wlr_box is defined in wlr/util/box.h. The C TUs that include wlroots
 * headers will get it automatically. For C++ (main.cpp) which does not
 * include wlroots headers, we include box.h directly — it has no [static N].
 */
#include <wlr/util/box.h>

struct wlr_renderer;
struct wlr_allocator;
struct wlr_compositor;
struct wlr_subcompositor;
struct wlr_data_device_manager;
struct wlr_xdg_shell;
struct wlr_xdg_surface;
struct wlr_xdg_toplevel;
struct wlr_seat;
struct wlr_cursor;
struct wlr_xcursor_manager;
struct wlr_output;
struct wlr_output_layout;
struct wlr_output_layout_output;
struct wlr_scene;
struct wlr_scene_tree;
struct wlr_scene_output;
struct wlr_scene_output_layout;
struct wlr_surface;
struct wlr_input_device;
struct wlr_keyboard;
struct xkb_keymap;
struct xkb_context;

#include "tx_config.h"

/* ── Cursor grab mode ─────────────────────────────────────────────────────── */
typedef enum {
    TX_CURSOR_PASSTHROUGH = 0,
    TX_CURSOR_MOVE,
    TX_CURSOR_RESIZE,
} TXCursorMode;

/* ── TXView: one XDG toplevel window ──────────────────────────────────────── */
struct TXView {
    struct TXServer        *server;
    struct wlr_xdg_toplevel *xdg_toplevel;
    struct wlr_scene_tree   *scene_tree;
    struct wlr_scene_tree   *surface_tree;

    /* Border rectangles */
    struct wlr_scene_rect   *border_top;
    struct wlr_scene_rect   *border_bottom;
    struct wlr_scene_rect   *border_left;
    struct wlr_scene_rect   *border_right;

    /* Geometry */
    int                      x, y;
    int                      width, height;

    /* Animation interpolation */
    float                    cur_x, cur_y;
    int                      target_x, target_y;
    int                      target_w, target_h;

    bool                     is_floating;
    bool                     is_mapped;

    /* Intrusive linked list (singly-linked) */
    struct TXView           *next;

    /* Signal listeners */
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
};

/* ── TXOutput: one physical monitor ──────────────────────────────────────── */
struct TXOutput {
    struct TXServer      *server;
    struct wlr_output    *output;
    struct wlr_scene_output *scene_output;

    struct TXOutput      *next;

    struct wl_listener frame;
    struct wl_listener request_state;
    struct wl_listener destroy;
};

/* ── TXKeyboard: one keyboard device ─────────────────────────────────────── */
struct TXKeyboard {
    struct TXServer   *server;
    struct wlr_keyboard *keyboard;

    struct TXKeyboard *next;

    struct wl_listener key;
    struct wl_listener modifiers;
    struct wl_listener destroy;
};

/* ── Grab geometry for resize ────────────────────────────────────────────── */
struct TXGrabBox { int x, y, width, height; };

/* ── TXServer: central compositor state ──────────────────────────────────── */
struct TXServer {
    struct wl_display              *display;
    struct wl_event_loop           *event_loop;
    struct wlr_backend             *backend;
    struct wlr_renderer            *renderer;
    struct wlr_allocator           *allocator;
    struct wlr_compositor          *compositor;
    struct wlr_subcompositor       *subcompositor;
    struct wlr_data_device_manager *data_device_mgr;
    struct wlr_xdg_shell           *xdg_shell;
    struct wlr_output_layout       *output_layout;
    struct wlr_scene               *scene;
    struct wlr_scene_output_layout *scene_output_layout;
    struct wlr_seat                *seat;
    struct wlr_cursor              *cursor;
    struct wlr_xcursor_manager     *cursor_mgr;

    TXConfig                        config;
    struct wl_event_source         *anim_timer;

    TXCursorMode                    cursor_mode;
    struct TXView                  *grabbed_view;
    char                            wayland_socket[64]; /* our WAYLAND_DISPLAY */
    double                          grab_x, grab_y;
    struct TXGrabBox                grab_geobox;
    uint32_t                        resize_edges;

    /* Intrusive linked lists */
    struct TXOutput    *outputs;
    struct TXView      *views;
    struct TXKeyboard  *keyboards;

    /* Signal listeners */
    struct wl_listener new_output;
    struct wl_listener new_xdg_toplevel;
    struct wl_listener new_xdg_popup;
    struct wl_listener new_input;
    struct wl_listener request_cursor;
    struct wl_listener request_set_selection;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;
};

/* ── C API exposed to main.cpp (C++) ─────────────────────────────────────── */
bool             tx_server_init(struct TXServer *server);
void             tx_server_run(struct TXServer *server);
void             tx_server_destroy(struct TXServer *server);
void             tx_server_focus_view(struct TXServer *server,
                                       struct TXView *view,
                                       struct wlr_surface *surface);
struct TXView   *tx_server_view_at(struct TXServer *server,
                                    double lx, double ly,
                                    struct wlr_surface **surface_out,
                                    double *sx_out, double *sy_out);

/* Dynamic tiling and layout management */
void             tx_server_relayout(struct TXServer *server);
void             tx_server_focus_next(struct TXServer *server, int direction);
void             tx_server_swap_focused(struct TXServer *server, int direction);
void             tx_server_toggle_floating(struct TXServer *server);
void             tx_view_update_borders(struct TXView *view, bool active);
void             tx_view_set_geometry(struct TXView *view, int x, int y, int w, int h);

#ifdef __cplusplus
} /* extern "C" */
#endif
