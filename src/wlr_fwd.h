/*
 * wlr_backend.h – Pure C wrapper declarations for wlroots
 *
 * All wlroots headers are ONLY included in *.c files (compiled as C).
 * C++ source files include only THIS header for all wlroots types and
 * functions — this header forward-declares the C structs as opaque pointers
 * so C++ never touches the problematic [static N] array syntax.
 *
 * Architecture:
 *   C++ files  → include wlr_backend.h (this file)
 *   C files    → include actual <wlr/*.h> headers
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque C struct re-exports (defined in wlroots, used in C++ as pointers) */
struct wl_display;
struct wl_event_loop;
struct wl_listener;
struct wl_signal;
struct wl_list;
struct wl_client;

struct wlr_backend;
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
struct wlr_output_mode;
struct wlr_scene;
struct wlr_scene_tree;
struct wlr_scene_output;
struct wlr_scene_output_layout;
struct wlr_scene_node;
struct wlr_scene_buffer;
struct wlr_scene_surface;
struct wlr_scene_rect;
struct wlr_input_device;
struct wlr_keyboard;
struct wlr_pointer;
struct wlr_surface;
struct xkb_state;
struct xkb_keymap;
struct xkb_context;
struct pixman_region32;

typedef unsigned int  xkb_keysym_t;
typedef unsigned int  xkb_keycode_t;
typedef unsigned int  xkb_mod_mask_t;

#ifdef __cplusplus
}
#endif
