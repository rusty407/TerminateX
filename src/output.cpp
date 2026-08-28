/* ──────────────────────────────────────────────────────────────────────────────
 * output.cpp – Compiled as C. Monitor lifecycle, scene-graph frame commits.
 * ──────────────────────────────────────────────────────────────────────────── */

#define WLR_USE_UNSTABLE
#define _GNU_SOURCE

#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "tx_server.h"

#include <stdlib.h>
#include <time.h>

static void on_output_frame(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXOutput *txout = wl_container_of(listener, txout, frame);
    wlr_scene_output_commit(txout->scene_output, NULL);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(txout->scene_output, &now);
}

static void on_request_state(struct wl_listener *listener, void *data) {
    struct TXOutput *txout = wl_container_of(listener, txout, request_state);
    const struct wlr_output_event_request_state *event =
        (const struct wlr_output_event_request_state *)data;
    wlr_output_commit_state(txout->output, event->state);
}

static void on_output_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct TXOutput *txout = wl_container_of(listener, txout, destroy);

    wl_list_remove(&txout->frame.link);
    wl_list_remove(&txout->request_state.link);
    wl_list_remove(&txout->destroy.link);

    /* Remove from server linked list */
    struct TXServer *server = txout->server;
    struct TXOutput **pp = &server->outputs;
    while (*pp && *pp != txout) pp = &(*pp)->next;
    if (*pp) *pp = txout->next;
    free(txout);
}

void handle_new_output(struct TXServer *server, struct wlr_output *output) {
    wlr_log(WLR_INFO, "New output: %s", output->name);

    wlr_output_init_render(output, server->allocator, server->renderer);

    {
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_enabled(&state, true);
        struct wlr_output_mode *mode = wlr_output_preferred_mode(output);
        if (mode) wlr_output_state_set_mode(&state, mode);
        wlr_output_commit_state(output, &state);
        wlr_output_state_finish(&state);
    }

    struct TXOutput *txout = calloc(1, sizeof(*txout));
    txout->server = server;
    txout->output = output;

    txout->frame.notify          = on_output_frame;
    txout->request_state.notify  = on_request_state;
    txout->destroy.notify        = on_output_destroy;

    wl_signal_add(&output->events.frame,         &txout->frame);
    wl_signal_add(&output->events.request_state, &txout->request_state);
    wl_signal_add(&output->events.destroy,       &txout->destroy);

    struct wlr_output_layout_output *layout_output =
        wlr_output_layout_add_auto(server->output_layout, output);

    txout->scene_output = wlr_scene_output_create(server->scene, output);
    wlr_scene_output_layout_add_output(server->scene_output_layout,
                                       layout_output, txout->scene_output);

    /* Prepend to server list */
    txout->next = server->outputs;
    server->outputs = txout;
}
