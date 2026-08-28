/* ──────────────────────────────────────────────────────────────────────────────
 * input.cpp – Compiled as C. Keyboard, pointer, cursor, seat, keybind dispatch.
 * ──────────────────────────────────────────────────────────────────────────── */

#define WLR_USE_UNSTABLE
#define _GNU_SOURCE

#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/util/edges.h>
#include <xkbcommon/xkbcommon.h>

#include "tx_server.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* ── Keybind table (pure C, static array) ────────────────────────────────── */
#define MOD_SHIFT (1u << 0)   /* WLR_MODIFIER_SHIFT / Shift key */
#define MOD_ALT   (1u << 3)   /* WLR_MODIFIER_ALT / Alt key */
#define MOD_SUPER (1u << 6)   /* WLR_MODIFIER_LOGO / Super key */

typedef void (*KeyAction)(struct TXServer *);

typedef struct {
    uint32_t   mods;
    xkb_keysym_t sym;
    KeyAction  action;
} TXKeybind;

static void action_terminal(struct TXServer *srv) {
    wlr_log(WLR_INFO, "action_terminal triggered! Spawning terminal on WAYLAND_DISPLAY=%s",
            srv->wayland_socket[0] ? srv->wayland_socket : "default");
    if (fork() == 0) {
        unsetenv("LD_PRELOAD");
        if (srv->wayland_socket[0]) {
            setenv("WAYLAND_DISPLAY", srv->wayland_socket, 1);
        }
        if (srv->config.terminal[0]) {
            execlp(srv->config.terminal, srv->config.terminal, NULL);
        }
        const char *terms[] = { "kitty", "foot", "alacritty", "mousepad", "ghostty", "xterm", NULL };
        for (const char **t = terms; *t; ++t) execlp(*t, *t, NULL);
        _exit(1);
    }
}

static void action_close(struct TXServer *srv) {
    struct wlr_surface *focused = srv->seat->keyboard_state.focused_surface;
    if (!focused) {
        if (srv->views && srv->views->xdg_toplevel) {
            wlr_xdg_toplevel_send_close(srv->views->xdg_toplevel);
        }
        return;
    }
    for (struct TXView *v = srv->views; v; v = v->next) {
        if (v->is_mapped && v->xdg_toplevel && v->xdg_toplevel->base->surface == focused) {
            wlr_log(WLR_INFO, "action_close sending close to %s",
                    v->xdg_toplevel->app_id ? v->xdg_toplevel->app_id : "(unnamed)");
            wlr_xdg_toplevel_send_close(v->xdg_toplevel);
            break;
        }
    }
}

static void action_fullscreen(struct TXServer *srv) {
    struct wlr_surface *focused = srv->seat->keyboard_state.focused_surface;
    if (!focused) return;
    for (struct TXView *v = srv->views; v; v = v->next) {
        if (v->is_mapped && v->xdg_toplevel && v->xdg_toplevel->base->surface == focused) {
            bool cur = v->xdg_toplevel->current.fullscreen;
            wlr_log(WLR_INFO, "action_fullscreen toggled to %d", !cur);
            wlr_xdg_toplevel_set_fullscreen(v->xdg_toplevel, !cur);
            break;
        }
    }
}

static void action_focus_next(struct TXServer *srv) {
    tx_server_focus_next(srv, +1);
}

static void action_focus_prev(struct TXServer *srv) {
    tx_server_focus_next(srv, -1);
}

static void action_swap_next(struct TXServer *srv) {
    tx_server_swap_focused(srv, +1);
}

static void action_swap_prev(struct TXServer *srv) {
    tx_server_swap_focused(srv, -1);
}

static void action_toggle_floating(struct TXServer *srv) {
    tx_server_toggle_floating(srv);
}

static void action_quit(struct TXServer *srv) {
    wlr_log(WLR_INFO, "action_quit triggered");
    wl_display_terminate(srv->display);
}

static const TXKeybind KEYBINDS[] = {
    /* Alt bindings (Great for nested testing without host Hyprland collisions) */
    { MOD_ALT,                   XKB_KEY_Return,    action_terminal        },
    { MOD_ALT,                   XKB_KEY_KP_Enter,  action_terminal        },
    { MOD_ALT,                   XKB_KEY_q,         action_close           },
    { MOD_ALT,                   XKB_KEY_j,         action_focus_next      },
    { MOD_ALT,                   XKB_KEY_Down,      action_focus_next      },
    { MOD_ALT,                   XKB_KEY_k,         action_focus_prev      },
    { MOD_ALT,                   XKB_KEY_Up,        action_focus_prev      },
    { MOD_ALT | MOD_SHIFT,       XKB_KEY_j,         action_swap_next       },
    { MOD_ALT | MOD_SHIFT,       XKB_KEY_J,         action_swap_next       },
    { MOD_ALT | MOD_SHIFT,       XKB_KEY_k,         action_swap_prev       },
    { MOD_ALT | MOD_SHIFT,       XKB_KEY_K,         action_swap_prev       },
    { MOD_ALT,                   XKB_KEY_space,     action_toggle_floating },
    { MOD_ALT,                   XKB_KEY_v,         action_toggle_floating },
    { MOD_ALT,                   XKB_KEY_f,         action_fullscreen      },
    { MOD_ALT,                   XKB_KEY_Escape,    action_quit            },

    /* Super / Win key bindings (For standalone session) */
    { MOD_SUPER,                 XKB_KEY_Return,    action_terminal        },
    { MOD_SUPER,                 XKB_KEY_KP_Enter,  action_terminal        },
    { MOD_SUPER,                 XKB_KEY_q,         action_close           },
    { MOD_SUPER,                 XKB_KEY_j,         action_focus_next      },
    { MOD_SUPER,                 XKB_KEY_Down,      action_focus_next      },
    { MOD_SUPER,                 XKB_KEY_k,         action_focus_prev      },
    { MOD_SUPER,                 XKB_KEY_Up,        action_focus_prev      },
    { MOD_SUPER | MOD_SHIFT,     XKB_KEY_j,         action_swap_next       },
    { MOD_SUPER | MOD_SHIFT,     XKB_KEY_J,         action_swap_next       },
    { MOD_SUPER | MOD_SHIFT,     XKB_KEY_k,         action_swap_prev       },
    { MOD_SUPER | MOD_SHIFT,     XKB_KEY_K,         action_swap_prev       },
    { MOD_SUPER,                 XKB_KEY_space,     action_toggle_floating },
    { MOD_SUPER,                 XKB_KEY_v,         action_toggle_floating },
    { MOD_SUPER,                 XKB_KEY_f,         action_fullscreen      },
    { MOD_SUPER,                 XKB_KEY_Escape,    action_quit            },
};
static const int NKEYBINDS = (int)(sizeof(KEYBINDS) / sizeof(KEYBINDS[0]));

static bool handle_keybind(struct TXServer *srv, uint32_t mods, xkb_keysym_t sym) {
    uint32_t cleaned_mods = mods & (MOD_SHIFT | MOD_ALT | MOD_SUPER);
    for (int i = 0; i < NKEYBINDS; ++i) {
        if (cleaned_mods == KEYBINDS[i].mods && KEYBINDS[i].sym == sym) {
            KEYBINDS[i].action(srv);
            return true;
        }
    }
    return false;
}

/* ── Keyboard signal handlers ────────────────────────────────────────────── */
static void on_keyboard_key(struct wl_listener *listener, void *data) {
    struct TXKeyboard *txkb = wl_container_of(listener, txkb, key);
    struct TXServer   *server = txkb->server;
    const struct wlr_keyboard_key_event *event =
        (const struct wlr_keyboard_key_event *)data;
    struct wlr_keyboard *kb = txkb->keyboard;

    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *syms = NULL;
    int nsyms = xkb_state_key_get_syms(kb->xkb_state, keycode, &syms);

    bool handled = false;
    uint32_t mods = wlr_keyboard_get_modifiers(kb);

    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (int i = 0; i < nsyms; ++i) {
            char sym_name[64] = {0};
            xkb_keysym_get_name(syms[i], sym_name, sizeof(sym_name));
            wlr_log(WLR_DEBUG, "Key pressed: sym=%s (0x%x), mods=0x%x", sym_name, syms[i], mods);

            if (handle_keybind(server, mods, syms[i])) {
                handled = true;
                break;
            }
        }
    }

    if (!handled) {
        wlr_seat_set_keyboard(server->seat, kb);
        wlr_seat_keyboard_notify_key(server->seat,
                                     event->time_msec,
                                     event->keycode,
                                     event->state);
    }
}

static void on_keyboard_modifiers(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXKeyboard *txkb = wl_container_of(listener, txkb, modifiers);
    wlr_seat_set_keyboard(txkb->server->seat, txkb->keyboard);
    wlr_seat_keyboard_notify_modifiers(txkb->server->seat,
                                       &txkb->keyboard->modifiers);
}

static void on_keyboard_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXKeyboard *txkb = wl_container_of(listener, txkb, destroy);
    wl_list_remove(&txkb->key.link);
    wl_list_remove(&txkb->modifiers.link);
    wl_list_remove(&txkb->destroy.link);

    struct TXServer *server = txkb->server;
    struct TXKeyboard **pp = &server->keyboards;
    while (*pp && *pp != txkb) pp = &(*pp)->next;
    if (*pp) *pp = txkb->next;
    free(txkb);
}

static void add_keyboard(struct TXServer *server, struct wlr_input_device *device) {
    struct wlr_keyboard *kb = wlr_keyboard_from_input_device(device);

    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap  *map = xkb_keymap_new_from_names(ctx, NULL,
                                                         XKB_KEYMAP_COMPILE_NO_FLAGS);
    wlr_keyboard_set_keymap(kb, map);
    xkb_keymap_unref(map);
    xkb_context_unref(ctx);

    wlr_keyboard_set_repeat_info(kb, 25, 600);

    struct TXKeyboard *txkb = calloc(1, sizeof(*txkb));
    txkb->server   = server;
    txkb->keyboard = kb;

    txkb->key.notify        = on_keyboard_key;
    txkb->modifiers.notify  = on_keyboard_modifiers;
    txkb->destroy.notify    = on_keyboard_destroy;

    wl_signal_add(&kb->events.key,         &txkb->key);
    wl_signal_add(&kb->events.modifiers,   &txkb->modifiers);
    wl_signal_add(&device->events.destroy, &txkb->destroy);

    txkb->next = server->keyboards;
    server->keyboards = txkb;

    wlr_seat_set_keyboard(server->seat, kb);
}

/* ── process_cursor_motion ───────────────────────────────────────────────── */
static void process_cursor_motion(struct TXServer *server, uint32_t time_msec) {
    if (server->cursor_mode == TX_CURSOR_MOVE) {
        server->grabbed_view->x = (int)(server->cursor->x - server->grab_x);
        server->grabbed_view->y = (int)(server->cursor->y - server->grab_y);
        wlr_scene_node_set_position(&server->grabbed_view->scene_tree->node,
                                    server->grabbed_view->x,
                                    server->grabbed_view->y);
        return;
    }

    if (server->cursor_mode == TX_CURSOR_RESIZE) {
        double bx = server->cursor->x - server->grab_x;
        double by = server->cursor->y - server->grab_y;
        int new_left   = server->grab_geobox.x;
        int new_right  = server->grab_geobox.x + server->grab_geobox.width;
        int new_top    = server->grab_geobox.y;
        int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

        if (server->resize_edges & WLR_EDGE_TOP) {
            new_top = (int)by;
            if (new_top >= new_bottom) new_top = new_bottom - 1;
        } else if (server->resize_edges & WLR_EDGE_BOTTOM) {
            new_bottom = (int)by;
            if (new_bottom <= new_top) new_bottom = new_top + 1;
        }
        if (server->resize_edges & WLR_EDGE_LEFT) {
            new_left = (int)bx;
            if (new_left >= new_right) new_left = new_right - 1;
        } else if (server->resize_edges & WLR_EDGE_RIGHT) {
            new_right = (int)bx;
            if (new_right <= new_left) new_right = new_left + 1;
        }

        struct wlr_box geo;
        geo = server->grabbed_view->xdg_toplevel->base->geometry;
        server->grabbed_view->x = new_left - geo.x;
        server->grabbed_view->y = new_top  - geo.y;
        wlr_scene_node_set_position(&server->grabbed_view->scene_tree->node,
                                    server->grabbed_view->x,
                                    server->grabbed_view->y);
        wlr_xdg_toplevel_set_size(server->grabbed_view->xdg_toplevel,
                                   new_right - new_left,
                                   new_bottom - new_top);
        return;
    }

    /* Passthrough: hit-test and forward */
    struct wlr_surface *surface = NULL;
    double sx = 0, sy = 0;
    struct TXView *view = tx_server_view_at(server,
                                             server->cursor->x, server->cursor->y,
                                             &surface, &sx, &sy);
    if (!view)
        wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "left_ptr");

    if (surface) {
        wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server->seat, time_msec, sx, sy);

        // Hover-to-focus (focus-follows-mouse)
        if (server->config.focus_follows_mouse && view) {
            if (server->seat->keyboard_state.focused_surface != surface) {
                tx_server_focus_view(server, view, surface);
            }
        }
    } else {
        wlr_seat_pointer_notify_clear_focus(server->seat);
    }
}

/* ── Cursor signal handlers ──────────────────────────────────────────────── */
static void on_cursor_motion(struct wl_listener *listener, void *data) {
    struct TXServer *server = wl_container_of(listener, server, cursor_motion);
    const struct wlr_pointer_motion_event *ev =
        (const struct wlr_pointer_motion_event *)data;
    wlr_cursor_move(server->cursor, &ev->pointer->base, ev->delta_x, ev->delta_y);
    process_cursor_motion(server, ev->time_msec);
}

static void on_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct TXServer *server = wl_container_of(listener, server, cursor_motion_absolute);
    const struct wlr_pointer_motion_absolute_event *ev =
        (const struct wlr_pointer_motion_absolute_event *)data;
    wlr_cursor_warp_absolute(server->cursor, &ev->pointer->base, ev->x, ev->y);
    process_cursor_motion(server, ev->time_msec);
}

static void on_cursor_button(struct wl_listener *listener, void *data) {
    struct TXServer *server = wl_container_of(listener, server, cursor_button);
    const struct wlr_pointer_button_event *ev =
        (const struct wlr_pointer_button_event *)data;

    wlr_seat_pointer_notify_button(server->seat,
                                   ev->time_msec, ev->button, ev->state);

    if (ev->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        struct wlr_surface *surface = NULL;
        double sx = 0, sy = 0;
        struct TXView *view = tx_server_view_at(server,
                                                 server->cursor->x,
                                                 server->cursor->y,
                                                 &surface, &sx, &sy);
        if (view) tx_server_focus_view(server, view, surface);
    } else {
        if (server->cursor_mode != TX_CURSOR_PASSTHROUGH) {
            server->cursor_mode  = TX_CURSOR_PASSTHROUGH;
            server->grabbed_view = NULL;
        }
    }
}

static void on_cursor_axis(struct wl_listener *listener, void *data) {
    struct TXServer *server = wl_container_of(listener, server, cursor_axis);
    const struct wlr_pointer_axis_event *ev =
        (const struct wlr_pointer_axis_event *)data;
    wlr_seat_pointer_notify_axis(server->seat,
                                  ev->time_msec, ev->orientation,
                                  ev->delta, ev->delta_discrete,
                                  ev->source, ev->relative_direction);
}

static void on_cursor_frame(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXServer *server = wl_container_of(listener, server, cursor_frame);
    wlr_seat_pointer_notify_frame(server->seat);
}

static void on_request_cursor(struct wl_listener *listener, void *data) {
    struct TXServer *server = wl_container_of(listener, server, request_cursor);
    const struct wlr_seat_pointer_request_set_cursor_event *ev =
        (const struct wlr_seat_pointer_request_set_cursor_event *)data;
    if (server->seat->pointer_state.focused_client == ev->seat_client)
        wlr_cursor_set_surface(server->cursor, ev->surface,
                               ev->hotspot_x, ev->hotspot_y);
}

static void on_request_set_selection(struct wl_listener *listener, void *data) {
    struct TXServer *server = wl_container_of(listener, server, request_set_selection);
    const struct wlr_seat_request_set_selection_event *ev =
        (const struct wlr_seat_request_set_selection_event *)data;
    wlr_seat_set_selection(server->seat, ev->source, ev->serial);
}

/* ── handle_new_input (called from server.cpp) ───────────────────────────── */
void handle_new_input(struct TXServer *server, struct wlr_input_device *device) {
    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        wlr_log(WLR_INFO, "New keyboard: %s", device->name);
        add_keyboard(server, device);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        wlr_log(WLR_INFO, "New pointer: %s", device->name);
        wlr_cursor_attach_input_device(server->cursor, device);
        break;
    default:
        break;
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (server->keyboards) caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    wlr_seat_set_capabilities(server->seat, caps);
}

/* ── setup_cursor_signals (called from server.cpp) ───────────────────────── */
void setup_cursor_signals(struct TXServer *server) {
    server->cursor_motion.notify           = on_cursor_motion;
    server->cursor_motion_absolute.notify  = on_cursor_motion_absolute;
    server->cursor_button.notify           = on_cursor_button;
    server->cursor_axis.notify             = on_cursor_axis;
    server->cursor_frame.notify            = on_cursor_frame;
    server->request_cursor.notify          = on_request_cursor;
    server->request_set_selection.notify   = on_request_set_selection;

    wl_signal_add(&server->cursor->events.motion,
                  &server->cursor_motion);
    wl_signal_add(&server->cursor->events.motion_absolute,
                  &server->cursor_motion_absolute);
    wl_signal_add(&server->cursor->events.button,
                  &server->cursor_button);
    wl_signal_add(&server->cursor->events.axis,
                  &server->cursor_axis);
    wl_signal_add(&server->cursor->events.frame,
                  &server->cursor_frame);
    wl_signal_add(&server->seat->events.request_set_cursor,
                  &server->request_cursor);
    wl_signal_add(&server->seat->events.request_set_selection,
                  &server->request_set_selection);
}
