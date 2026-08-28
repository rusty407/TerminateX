/* ──────────────────────────────────────────────────────────────────────────────
 * tx_config.h – TerminateX Configuration header
 * ──────────────────────────────────────────────────────────────────────────── */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   border_size;
    int   gaps_in;
    int   gaps_out;
    float active_border[4];     /* RGBA [0.0 - 1.0] */
    float inactive_border[4];   /* RGBA [0.0 - 1.0] */
    bool  focus_follows_mouse;
    bool  animations;
    float animation_speed;
    char  terminal[64];
} TXConfig;

void tx_config_init_defaults(TXConfig *cfg);
void tx_config_load(TXConfig *cfg);

#ifdef __cplusplus
}
#endif
