/* ──────────────────────────────────────────────────────────────────────────────
 * view.cpp – Compiled as C. XDG toplevel lifecycle, focus, floating placement.
 * ──────────────────────────────────────────────────────────────────────────── */

#define WLR_USE_UNSTABLE
#define _GNU_SOURCE

#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include <wlr/util/edges.h>

#include "tx_server.h"

#include <stdlib.h>

/* ── Cascade counter for window placement ────────────────────────────────── */
static int s_cascade = 0;

/* ── Border and geometry helpers ─────────────────────────────────────────── */
void tx_view_update_borders(struct TXView *view, bool active) {
    if (!view || !view->border_top) return;
    const float *color = active ? view->server->config.active_border : view->server->config.inactive_border;
    wlr_scene_rect_set_color(view->border_top, color);
    wlr_scene_rect_set_color(view->border_bottom, color);
    wlr_scene_rect_set_color(view->border_left, color);
    wlr_scene_rect_set_color(view->border_right, color);
}

void tx_view_set_geometry(struct TXView *view, int x, int y, int w, int h) {
    if (!view) return;

    view->target_x = x;
    view->target_y = y;
    view->target_w = w;
    view->target_h = h;

    if (!view->server->config.animations || !view->is_mapped) {
        view->cur_x = (float)x;
        view->cur_y = (float)y;
        view->x = x;
        view->y = y;
        view->width = w;
        view->height = h;
        wlr_scene_node_set_position(&view->scene_tree->node, x, y);
    }

    int b = view->server->config.border_size;
    if (b < 0) b = 0;

    int content_w = w - 2 * b;
    int content_h = h - 2 * b;
    if (content_w < 10) content_w = 10;
    if (content_h < 10) content_h = 10;

    if (view->surface_tree) {
        wlr_scene_node_set_position(&view->surface_tree->node, b, b);
    }

    if (view->border_top && view->border_bottom && view->border_left && view->border_right) {
        if (b > 0) {
            wlr_scene_rect_set_size(view->border_top, w, b);
            wlr_scene_node_set_position(&view->border_top->node, 0, 0);

            wlr_scene_rect_set_size(view->border_bottom, w, b);
            wlr_scene_node_set_position(&view->border_bottom->node, 0, h - b > 0 ? h - b : 0);

            int side_h = h - 2 * b > 0 ? h - 2 * b : 0;
            wlr_scene_rect_set_size(view->border_left, b, side_h);
            wlr_scene_node_set_position(&view->border_left->node, 0, b);

            wlr_scene_rect_set_size(view->border_right, b, side_h);
            wlr_scene_node_set_position(&view->border_right->node, w - b > 0 ? w - b : 0, b);
        } else {
            wlr_scene_rect_set_size(view->border_top, 0, 0);
            wlr_scene_rect_set_size(view->border_bottom, 0, 0);
            wlr_scene_rect_set_size(view->border_left, 0, 0);
            wlr_scene_rect_set_size(view->border_right, 0, 0);
        }
    }

    wlr_xdg_toplevel_set_size(view->xdg_toplevel, content_w, content_h);
}

/* ── begin_interactive ───────────────────────────────────────────────────── */
static void begin_interactive(struct TXServer *server,
                               struct TXView   *view,
                               TXCursorMode     mode,
                               uint32_t         edges) {
    struct wlr_surface *focused = server->seat->pointer_state.focused_surface;
    if (view->xdg_toplevel->base->surface !=
        wlr_surface_get_root_surface(focused)) return;

    server->grabbed_view = view;
    server->cursor_mode  = mode;

    if (mode == TX_CURSOR_MOVE) {
        server->grab_x = server->cursor->x - view->x;
        server->grab_y = server->cursor->y - view->y;
    } else {
        struct wlr_box geo = view->xdg_toplevel->base->geometry;

        double border_x = (view->x + geo.x) +
            ((edges & WLR_EDGE_RIGHT)  ? geo.width  : 0);
        double border_y = (view->y + geo.y) +
            ((edges & WLR_EDGE_BOTTOM) ? geo.height : 0);

        server->grab_x = server->cursor->x - border_x;
        server->grab_y = server->cursor->y - border_y;

        server->grab_geobox.x      = view->x + geo.x;
        server->grab_geobox.y      = view->y + geo.y;
        server->grab_geobox.width  = geo.width;
        server->grab_geobox.height = geo.height;
        server->resize_edges       = edges;
    }
}

/* ── tx_server_relayout: Hyprland-style dynamic tiling layout ────────────── */
void tx_server_relayout(struct TXServer *server) {
    if (!server) return;

    int outer_gap = server->config.gaps_out;
    int inner_gap = server->config.gaps_in;

    // 1. Determine usable screen area from output layout
    int screen_w = 1280, screen_h = 720;
    if (server->outputs && server->outputs->output) {
        screen_w = server->outputs->output->width;
        screen_h = server->outputs->output->height;
    }

    // 2. Collect all mapped tiled views in layout order
    struct TXView *tiled[64];
    int n_tiled = 0;

    for (struct TXView *v = server->views; v; v = v->next) {
        if (v->is_mapped && !v->is_floating && v->xdg_toplevel) {
            if (n_tiled < 64) {
                tiled[n_tiled++] = v;
            }
        }
    }

    if (n_tiled == 0) return;

    wlr_log(WLR_INFO, "Relayout: %d tiled views across %dx%d (gaps: out=%d in=%d border=%d)",
            n_tiled, screen_w, screen_h, outer_gap, inner_gap, server->config.border_size);

    // Single window: Expands to full screen with outer gaps
    if (n_tiled == 1) {
        struct TXView *v = tiled[0];
        int x = outer_gap;
        int y = outer_gap;
        int w = screen_w - 2 * outer_gap;
        int h = screen_h - 2 * outer_gap;
        if (w < 100) w = 100;
        if (h < 100) h = 100;

        tx_view_set_geometry(v, x, y, w, h);
        return;
    }

    // Two windows: Split 50/50 side-by-side
    if (n_tiled == 2) {
        int avail_w = screen_w - 2 * outer_gap - inner_gap;
        int w = avail_w / 2;
        int h = screen_h - 2 * outer_gap;
        if (w < 50) w = 50;
        if (h < 50) h = 50;

        // Master (Left)
        tx_view_set_geometry(tiled[0], outer_gap, outer_gap, w, h);

        // Stack (Right)
        tx_view_set_geometry(tiled[1], outer_gap + w + inner_gap, outer_gap, w, h);
        return;
    }

    // 3+ Windows: Master-Stack Layout
    int avail_w = screen_w - 2 * outer_gap - inner_gap;
    int master_w = avail_w / 2;
    int stack_w  = avail_w - master_w;
    int master_h = screen_h - 2 * outer_gap;

    // Master window (Left 50%)
    tx_view_set_geometry(tiled[0], outer_gap, outer_gap, master_w, master_h);

    // Stack column (Right 50%, balanced vertically for remaining N-1 windows)
    int n_stack = n_tiled - 1;
    int avail_stack_h = screen_h - 2 * outer_gap - (n_stack - 1) * inner_gap;
    int item_h = avail_stack_h / n_stack;

    for (int i = 0; i < n_stack; ++i) {
        struct TXView *vi = tiled[1 + i];
        int item_y = outer_gap + i * (item_h + inner_gap);
        int cur_h = (i == n_stack - 1) ? (screen_h - outer_gap - item_y) : item_h;
        if (cur_h < 30) cur_h = 30;

        tx_view_set_geometry(vi, outer_gap + master_w + inner_gap, item_y, stack_w, cur_h);
    }
}

/* ── View signal handlers ────────────────────────────────────────────────── */
static void on_map(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXView *view = wl_container_of(listener, view, map);
    view->is_mapped = true;
    wlr_log(WLR_INFO, "Mapped XDG toplevel: %s",
            view->xdg_toplevel->app_id ? view->xdg_toplevel->app_id : "(unnamed)");

    tx_server_relayout(view->server);
    tx_server_focus_view(view->server, view, view->xdg_toplevel->base->surface);
}

static void on_unmap(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXView *view = wl_container_of(listener, view, unmap);
    view->is_mapped = false;
    if (view == view->server->grabbed_view) {
        view->server->cursor_mode  = TX_CURSOR_PASSTHROUGH;
        view->server->grabbed_view = NULL;
    }
    tx_server_relayout(view->server);
}

static void on_commit(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXView *view = wl_container_of(listener, view, commit);
    if (view->xdg_toplevel->base->initial_commit) {
        wlr_log(WLR_INFO, "Initial commit for toplevel %s, configuring size 0x0",
                view->xdg_toplevel->app_id ? view->xdg_toplevel->app_id : "(unnamed)");
        wlr_xdg_toplevel_set_size(view->xdg_toplevel, 0, 0);
    }
}

static void on_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXView *view = wl_container_of(listener, view, destroy);

    wl_list_remove(&view->map.link);
    wl_list_remove(&view->unmap.link);
    wl_list_remove(&view->commit.link);
    wl_list_remove(&view->destroy.link);
    wl_list_remove(&view->request_move.link);
    wl_list_remove(&view->request_resize.link);
    wl_list_remove(&view->request_maximize.link);
    wl_list_remove(&view->request_fullscreen.link);

    struct TXServer *server = view->server;
    struct TXView **pp = &server->views;
    while (*pp && *pp != view) pp = &(*pp)->next;
    if (*pp) *pp = view->next;
    free(view);

    tx_server_relayout(server);
}

static void on_request_move(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXView *view = wl_container_of(listener, view, request_move);
    begin_interactive(view->server, view, TX_CURSOR_MOVE, 0);
}

static void on_request_resize(struct wl_listener *listener, void *data) {
    struct TXView *view = wl_container_of(listener, view, request_resize);
    const struct wlr_xdg_toplevel_resize_event *event =
        (const struct wlr_xdg_toplevel_resize_event *)data;
    begin_interactive(view->server, view, TX_CURSOR_RESIZE, event->edges);
}

static void on_request_maximize(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXView *view = wl_container_of(listener, view, request_maximize);
    wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);
}

static void on_request_fullscreen(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXView *view = wl_container_of(listener, view, request_fullscreen);
    wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);
}

/* ── handle_new_xdg_toplevel (called from server.cpp) ───────────────────── */
void handle_new_xdg_toplevel(struct TXServer *server,
                              struct wlr_xdg_toplevel *toplevel) {
    wlr_log(WLR_INFO, "New XDG toplevel: %s", toplevel->app_id ? toplevel->app_id : "(unnamed)");

    struct TXView *view = calloc(1, sizeof(*view));
    view->server       = server;
    view->xdg_toplevel = toplevel;

    // Parent container tree for the window and its borders
    view->scene_tree = wlr_scene_tree_create(&server->scene->tree);
    view->scene_tree->node.data = view;

    // 4 border rectangles
    view->border_top    = wlr_scene_rect_create(view->scene_tree, 1, 1, server->config.inactive_border);
    view->border_bottom = wlr_scene_rect_create(view->scene_tree, 1, 1, server->config.inactive_border);
    view->border_left   = wlr_scene_rect_create(view->scene_tree, 1, 1, server->config.inactive_border);
    view->border_right  = wlr_scene_rect_create(view->scene_tree, 1, 1, server->config.inactive_border);

    // Inner surface tree offset by border thickness
    view->surface_tree  = wlr_scene_xdg_surface_create(view->scene_tree, toplevel->base);
    toplevel->base->data = view->scene_tree;

    int b = server->config.border_size;
    wlr_scene_node_set_position(&view->surface_tree->node, b, b);

    view->x = 50 + s_cascade * 20;
    view->y = 50 + s_cascade * 20;
    view->cur_x = (float)view->x;
    view->cur_y = (float)view->y;
    view->target_x = view->x;
    view->target_y = view->y;
    s_cascade = (s_cascade + 1) % 15;
    wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);

    view->map.notify              = on_map;
    view->unmap.notify            = on_unmap;
    view->commit.notify           = on_commit;
    view->destroy.notify          = on_destroy;
    view->request_move.notify     = on_request_move;
    view->request_resize.notify   = on_request_resize;
    view->request_maximize.notify = on_request_maximize;
    view->request_fullscreen.notify = on_request_fullscreen;

    wl_signal_add(&toplevel->base->surface->events.map,    &view->map);
    wl_signal_add(&toplevel->base->surface->events.unmap,  &view->unmap);
    wl_signal_add(&toplevel->base->surface->events.commit, &view->commit);
    wl_signal_add(&toplevel->events.destroy,               &view->destroy);
    wl_signal_add(&toplevel->events.request_move,          &view->request_move);
    wl_signal_add(&toplevel->events.request_resize,        &view->request_resize);
    wl_signal_add(&toplevel->events.request_maximize,      &view->request_maximize);
    wl_signal_add(&toplevel->events.request_fullscreen,    &view->request_fullscreen);

    /* Prepend to server list */
    view->next = server->views;
    server->views = view;
}
