// ──────────────────────────────────────────────────────────────────────────────
// server.cpp – Compiled as C. TXServer init, Wayland display, backend, signals.
// ──────────────────────────────────────────────────────────────────────────────
// NOTE: This file is compiled as C (LANGUAGE C in CMake) to allow wlroots
// headers with C99 [static N] array parameters to be parsed correctly.
// C++ features (std::list, lambdas, etc.) are NOT used here.

#define WLR_USE_UNSTABLE
#define _GNU_SOURCE

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "tx_server.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ── Forward declarations for handlers in other C TUs ─────────────────────────
void handle_new_output(struct TXServer *server, struct wlr_output *output);
void handle_new_xdg_toplevel(struct TXServer *server, struct wlr_xdg_toplevel *toplevel);
void handle_new_input(struct TXServer *server, struct wlr_input_device *device);
void setup_cursor_signals(struct TXServer *server);

// ── Signal dispatchers ────────────────────────────────────────────────────────
static void on_new_output(struct wl_listener *listener, void *data) {
    struct TXServer *server = wl_container_of(listener, server, new_output);
    handle_new_output(server, (struct wlr_output *)data);
}

static void on_new_xdg_toplevel(struct wl_listener *listener, void *data) {
    struct TXServer *server = wl_container_of(listener, server, new_xdg_toplevel);
    handle_new_xdg_toplevel(server, (struct wlr_xdg_toplevel *)data);
}

static void on_new_xdg_popup(struct wl_listener *listener, void *data) {
    struct TXServer *server = wl_container_of(listener, server, new_xdg_popup);
    struct wlr_xdg_popup *popup = (struct wlr_xdg_popup *)data;

    struct wlr_scene_tree *parent_tree = NULL;
    if (popup->parent) {
        struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(popup->parent);
        if (parent && parent->data) {
            parent_tree = (struct wlr_scene_tree *)parent->data;
        }
    }
    if (!parent_tree) {
        parent_tree = &server->scene->tree;
    }
    popup->base->data = wlr_scene_xdg_surface_create(parent_tree, popup->base);
}

static void on_new_input(struct wl_listener *listener, void *data) {
    struct TXServer *server = wl_container_of(listener, server, new_input);
    handle_new_input(server, (struct wlr_input_device *)data);
}

#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>

#include <math.h>

static int on_anim_tick(void *data) {
    struct TXServer *server = (struct TXServer *)data;
    if (!server) return 0;

    float speed = server->config.animation_speed;

    for (struct TXView *v = server->views; v; v = v->next) {
        if (!v->is_mapped || v->is_floating) continue;

        float dx = (float)v->target_x - v->cur_x;
        float dy = (float)v->target_y - v->cur_y;

        if (fabsf(dx) > 0.5f || fabsf(dy) > 0.5f) {
            v->cur_x += dx * speed;
            v->cur_y += dy * speed;
            v->x = (int)roundf(v->cur_x);
            v->y = (int)roundf(v->cur_y);
            wlr_scene_node_set_position(&v->scene_tree->node, v->x, v->y);
        } else {
            v->cur_x = (float)v->target_x;
            v->cur_y = (float)v->target_y;
            v->x = v->target_x;
            v->y = v->target_y;
            wlr_scene_node_set_position(&v->scene_tree->node, v->x, v->y);
        }
    }

    wl_event_source_timer_update(server->anim_timer, 16);
    return 0;
}

// ── tx_server_init ────────────────────────────────────────────────────────────
bool tx_server_init(struct TXServer *server) {
    memset(server, 0, sizeof(*server));
    server->cursor_mode = TX_CURSOR_PASSTHROUGH;

    // Load configuration
    tx_config_load(&server->config);

    // 1. Wayland display
    server->display = wl_display_create();
    if (!server->display) { wlr_log(WLR_ERROR, "Failed to create wl_display"); return false; }
    server->event_loop = wl_display_get_event_loop(server->display);

    // Animation timer (60 FPS tick)
    if (server->config.animations) {
        server->anim_timer = wl_event_loop_add_timer(server->event_loop, on_anim_tick, server);
        wl_event_source_timer_update(server->anim_timer, 16);
    }

    // 2. Backend – auto-detects nested Wayland/X11 or DRM/KMS
    server->backend = wlr_backend_autocreate(server->event_loop, NULL);
    if (!server->backend) { wlr_log(WLR_ERROR, "Failed to create backend"); return false; }

    // 3. Renderer + allocator
    server->renderer = wlr_renderer_autocreate(server->backend);
    if (!server->renderer) { wlr_log(WLR_ERROR, "Failed to create renderer"); return false; }
    wlr_renderer_init_wl_display(server->renderer, server->display);
    wlr_renderer_init_wl_shm(server->renderer, server->display);

    server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
    if (!server->allocator) { wlr_log(WLR_ERROR, "Failed to create allocator"); return false; }

    // 4. Core Wayland protocols
    server->compositor     = wlr_compositor_create(server->display, 5, server->renderer);
    server->subcompositor  = wlr_subcompositor_create(server->display);
    server->data_device_mgr = wlr_data_device_manager_create(server->display);

    // DMA-BUF for GPU/EGL client acceleration (e.g. Kitty, Ghostty, Alacritty)
    wlr_linux_dmabuf_v1_create_with_renderer(server->display, 4, server->renderer);

    // 5. Output layout
    server->output_layout = wlr_output_layout_create(server->display);
    wlr_xdg_output_manager_v1_create(server->display, server->output_layout);

    // 6. Scene graph
    server->scene = wlr_scene_create();
    server->scene_output_layout =
        wlr_scene_attach_output_layout(server->scene, server->output_layout);

    // 7. XDG shell + decoration manager
    server->xdg_shell = wlr_xdg_shell_create(server->display, 3);
    wlr_xdg_decoration_manager_v1_create(server->display, 1);

    // 8. Seat
    server->seat = wlr_seat_create(server->display, "seat0");

    // 9. Cursor
    server->cursor     = wlr_cursor_create();
    server->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    wlr_cursor_attach_output_layout(server->cursor, server->output_layout);
    wlr_xcursor_manager_load(server->cursor_mgr, 1.0f);

    // 10. Wire signals
    server->new_output.notify       = on_new_output;
    server->new_xdg_toplevel.notify = on_new_xdg_toplevel;
    server->new_xdg_popup.notify    = on_new_xdg_popup;
    server->new_input.notify        = on_new_input;

    wl_signal_add(&server->backend->events.new_output,      &server->new_output);
    wl_signal_add(&server->xdg_shell->events.new_toplevel,  &server->new_xdg_toplevel);
    wl_signal_add(&server->xdg_shell->events.new_popup,     &server->new_xdg_popup);
    wl_signal_add(&server->backend->events.new_input,       &server->new_input);

    setup_cursor_signals(server);

    // 11. Create Wayland socket
    const char *socket = wl_display_add_socket_auto(server->display);
    if (!socket) { wlr_log(WLR_ERROR, "Failed to create Wayland socket"); return false; }

    // Store socket name so keybind actions can pass it to forked children
    snprintf(server->wayland_socket, sizeof(server->wayland_socket), "%s", socket);

    // 12. Start backend
    if (!wlr_backend_start(server->backend)) {
        wlr_log(WLR_ERROR, "Failed to start backend");
        return false;
    }

    setenv("WAYLAND_DISPLAY", socket, 1);
    wlr_log(WLR_INFO, "TerminateX running on WAYLAND_DISPLAY=%s", socket);
    return true;
}

// ── tx_server_run ─────────────────────────────────────────────────────────────
void tx_server_run(struct TXServer *server) {
    wl_display_run(server->display);
}

// ── tx_server_destroy ─────────────────────────────────────────────────────────
void tx_server_destroy(struct TXServer *server) {
    if (server->anim_timer) {
        wl_event_source_remove(server->anim_timer);
        server->anim_timer = NULL;
    }
    wlr_xcursor_manager_destroy(server->cursor_mgr);
    wlr_cursor_destroy(server->cursor);
    wlr_scene_node_destroy(&server->scene->tree.node);
    wlr_output_layout_destroy(server->output_layout);
    wlr_backend_destroy(server->backend);
    wl_display_destroy_clients(server->display);
    wl_display_destroy(server->display);
}

// ── tx_server_focus_view ──────────────────────────────────────────────────────
void tx_server_focus_view(struct TXServer *server, struct TXView *view, struct wlr_surface *surface) {
    if (!view) return;

    struct wlr_surface *prev = server->seat->keyboard_state.focused_surface;
    if (prev && prev != surface) {
        struct TXView *v = server->views;
        while (v) {
            if (v->xdg_toplevel && v->xdg_toplevel->base->surface == prev) {
                wlr_xdg_toplevel_set_activated(v->xdg_toplevel, false);
                break;
            }
            v = v->next;
        }
    }

    wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);
    if (view->is_floating) {
        wlr_scene_node_raise_to_top(&view->scene_tree->node);
    }

    // Update border highlights across all views
    for (struct TXView *v = server->views; v; v = v->next) {
        tx_view_update_borders(v, (v == view));
    }

    struct wlr_keyboard *kb = wlr_seat_get_keyboard(server->seat);
    if (kb) {
        wlr_seat_keyboard_notify_enter(server->seat, surface,
                                       kb->keycodes, kb->num_keycodes,
                                       &kb->modifiers);
    }
}

// ── tx_server_view_at ─────────────────────────────────────────────────────────
struct TXView *tx_server_view_at(struct TXServer *server,
                                  double lx, double ly,
                                  struct wlr_surface **surface_out,
                                  double *sx_out, double *sy_out) {
    struct wlr_scene_node *node =
        wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx_out, sy_out);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER)
        return NULL;

    struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
    struct wlr_scene_surface *scene_surface =
        wlr_scene_surface_try_from_buffer(scene_buffer);
    if (!scene_surface) return NULL;

    *surface_out = scene_surface->surface;

    struct wlr_scene_tree *tree = node->parent;
    while (tree && tree->node.data == NULL)
        tree = tree->node.parent;

    return tree ? (struct TXView *)tree->node.data : NULL;
}

// ── Window navigation & management helpers ────────────────────────────────────
void tx_server_focus_next(struct TXServer *server, int direction) {
    if (!server || !server->views) return;

    struct TXView *mapped[64];
    int n = 0;
    int cur_idx = -1;
    struct wlr_surface *focused = server->seat->keyboard_state.focused_surface;

    for (struct TXView *v = server->views; v; v = v->next) {
        if (v->is_mapped && v->xdg_toplevel) {
            if (focused && v->xdg_toplevel->base->surface == focused) {
                cur_idx = n;
            }
            if (n < 64) mapped[n++] = v;
        }
    }

    if (n <= 1) return;

    int next_idx = (cur_idx + direction) % n;
    if (next_idx < 0) next_idx += n;

    struct TXView *target = mapped[next_idx];
    tx_server_focus_view(server, target, target->xdg_toplevel->base->surface);
}

void tx_server_swap_focused(struct TXServer *server, int direction) {
    if (!server || !server->views) return;

    struct TXView *mapped[64];
    int n = 0;
    int cur_idx = -1;
    struct wlr_surface *focused = server->seat->keyboard_state.focused_surface;

    for (struct TXView *v = server->views; v; v = v->next) {
        if (v->is_mapped && v->xdg_toplevel) {
            if (focused && v->xdg_toplevel->base->surface == focused) {
                cur_idx = n;
            }
            if (n < 64) mapped[n++] = v;
        }
    }

    if (n <= 1 || cur_idx == -1) return;

    int swap_idx = (cur_idx + direction) % n;
    if (swap_idx < 0) swap_idx += n;
    if (swap_idx == cur_idx) return;

    struct TXView *a = mapped[cur_idx];
    struct TXView *b = mapped[swap_idx];

    // Swap positions in the linked list
    // Reconstruct list from mapped array
    mapped[cur_idx] = b;
    mapped[swap_idx] = a;

    server->views = mapped[0];
    for (int i = 0; i < n - 1; ++i) {
        mapped[i]->next = mapped[i + 1];
    }
    mapped[n - 1]->next = NULL;

    tx_server_relayout(server);
    tx_server_focus_view(server, a, a->xdg_toplevel->base->surface);
}

void tx_server_toggle_floating(struct TXServer *server) {
    if (!server || !server->views) return;
    struct wlr_surface *focused = server->seat->keyboard_state.focused_surface;
    if (!focused) return;

    for (struct TXView *v = server->views; v; v = v->next) {
        if (v->is_mapped && v->xdg_toplevel && v->xdg_toplevel->base->surface == focused) {
            v->is_floating = !v->is_floating;
            wlr_log(WLR_INFO, "Toggled floating for %s: %d",
                    v->xdg_toplevel->app_id ? v->xdg_toplevel->app_id : "(unnamed)",
                    v->is_floating);
            if (v->is_floating) {
                // Set default floating size
                wlr_xdg_toplevel_set_size(v->xdg_toplevel, 640, 480);
            }
            tx_server_relayout(server);
            break;
        }
    }
}
