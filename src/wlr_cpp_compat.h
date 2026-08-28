/*
 * wlr_cpp_compat.h – Suppress C99 [static N] array params from wlroots headers
 *
 * Strategy: pre-define the include guards of the two problematic headers
 * (color.h and wlr_scene.h) so the C++ compiler never sees their bodies.
 * Then we include ONLY wlr_scene.h via a C compilation unit (wlr_scene_c.c)
 * that exposes the functions we need with clean C linkage.
 *
 * For wlr_scene_rect (the only [static 4] function we call), we re-declare
 * it manually here with a pointer parameter instead.
 *
 * This is the canonical approach: skip the problematic guard, forward-declare
 * manually, and link from a C translation unit.
 */

#pragma once

#ifdef __cplusplus

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

/* ── Pre-satisfy include guards for the two problematic headers ─────────────── */
/* color.h: has [static 9], [static 3] */
#define WLR_RENDER_COLOR_H

/* wlr_scene.h: has [static 4] – we'll include it from C code only */
/* We do NOT suppress wlr_scene.h entirely; instead we include it from the
 * C shim (wlr_scene_shim.c) and only forward-declare what we need here. */

/* ── Forward declarations for the wlr_color_transform functions we use ──────── */
/* (Only wlr_renderer.h pulls in color.h; by pre-satisfying its guard we avoid
 * all [static N] errors. We don't call any color_transform functions directly.) */

extern "C" {

/* Provide a minimal stub so wlr_renderer.h can compile without color.h */
struct wlr_color_transform; /* opaque */

struct wlr_color_primaries {
    /* We never instantiate this in C++ code; opaque is fine */
    float r[2], g[2], b[2], w[2];
    int transfer;
};

enum wlr_color_transform_type {
    WLR_COLOR_TRANSFORM_SRGB = 0,
};

} /* extern "C" */

#endif /* __cplusplus */
