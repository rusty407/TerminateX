/*
 * wlr_shim.c  –  Pure-C translation unit that includes ALL wlroots headers.
 *
 * This file is compiled as C (not C++) so the [static N] array param syntax
 * in color.h / wlr_scene.h is valid. It exposes no additional functions —
 * it simply acts as an object file that satisfies the linker for all wlroots
 * symbols used elsewhere.
 *
 * C++ source files must NOT include wlroots headers directly.
 * They must include "wlr_decls.h" which provides clean C++ declarations.
 */

#define WLR_USE_UNSTABLE
#define _GNU_SOURCE

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/color.h>
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
#include <wlr/util/log.h>
#include <wlr/util/edges.h>
#include <xkbcommon/xkbcommon.h>
