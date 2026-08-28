/* ──────────────────────────────────────────────────────────────────────────────
 * tx_config.c – Parses ~/.config/terminatex/terminatex.conf
 * ──────────────────────────────────────────────────────────────────────────── */

#define _GNU_SOURCE
#include "tx_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>

static void parse_color_hex(const char *val, float out[4]) {
    // Expecting 0xRRGGBBAA or 0xRRGGBB or #RRGGBBAA or #RRGGBB
    if (val[0] == '#') val++;
    else if (val[0] == '0' && (val[1] == 'x' || val[1] == 'X')) val += 2;

    unsigned int r = 255, g = 255, b = 255, a = 255;
    size_t len = strlen(val);
    if (len >= 8) {
        sscanf(val, "%02x%02x%02x%02x", &r, &g, &b, &a);
    } else if (len >= 6) {
        sscanf(val, "%02x%02x%02x", &r, &g, &b);
        a = 255;
    }

    out[0] = (float)r / 255.0f;
    out[1] = (float)g / 255.0f;
    out[2] = (float)b / 255.0f;
    out[3] = (float)a / 255.0f;
}

static char *trim(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void tx_config_init_defaults(TXConfig *cfg) {
    cfg->border_size = 2;
    cfg->gaps_in = 8;
    cfg->gaps_out = 12;

    // Active: Vibrant Cyan #33ccff (Hyprland style)
    cfg->active_border[0] = 0.20f;
    cfg->active_border[1] = 0.80f;
    cfg->active_border[2] = 1.00f;
    cfg->active_border[3] = 1.00f;

    // Inactive: Subtle Dark Gray #444444
    cfg->inactive_border[0] = 0.27f;
    cfg->inactive_border[1] = 0.27f;
    cfg->inactive_border[2] = 0.27f;
    cfg->inactive_border[3] = 1.00f;

    cfg->focus_follows_mouse = true;
    cfg->animations = true;
    cfg->animation_speed = 0.25f;
    snprintf(cfg->terminal, sizeof(cfg->terminal), "kitty");
}

static void create_default_config_file(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f,
        "# ─────────────────────────────────────────────────────────────\n"
        "# TerminateX Configuration (Hyprland / DWM style)\n"
        "# ─────────────────────────────────────────────────────────────\n"
        "\n"
        "# Window layout & gaps\n"
        "border_size = 2\n"
        "gaps_in = 8\n"
        "gaps_out = 12\n"
        "\n"
        "# Window border colors (Hex: 0xRRGGBBAA or #RRGGBBAA)\n"
        "active_border_color = 0x33ccffee\n"
        "inactive_border_color = 0x444444cc\n"
        "\n"
        "# Input & Focus behavior\n"
        "focus_follows_mouse = 1\n"
        "\n"
        "# Smooth animations (1 = enabled, 0 = disabled)\n"
        "animations = 1\n"
        "animation_speed = 0.25\n"
        "\n"
        "# Default terminal application\n"
        "terminal = kitty\n"
    );
    fclose(f);
}

void tx_config_load(TXConfig *cfg) {
    tx_config_init_defaults(cfg);

    const char *home = getenv("HOME");
    if (!home) return;

    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/.config/terminatex", home);
    mkdir(dir_path, 0755);

    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/terminatex.conf", dir_path);

    FILE *f = fopen(file_path, "r");
    if (!f) {
        create_default_config_file(file_path);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        if (*p == '#' || *p == '\0' || *p == ';') continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);

        if (strcmp(key, "border_size") == 0) {
            cfg->border_size = atoi(val);
        } else if (strcmp(key, "gaps_in") == 0) {
            cfg->gaps_in = atoi(val);
        } else if (strcmp(key, "gaps_out") == 0) {
            cfg->gaps_out = atoi(val);
        } else if (strcmp(key, "active_border_color") == 0) {
            parse_color_hex(val, cfg->active_border);
        } else if (strcmp(key, "inactive_border_color") == 0) {
            parse_color_hex(val, cfg->inactive_border);
        } else if (strcmp(key, "focus_follows_mouse") == 0) {
            cfg->focus_follows_mouse = (atoi(val) != 0);
        } else if (strcmp(key, "animations") == 0) {
            cfg->animations = (atoi(val) != 0);
        } else if (strcmp(key, "animation_speed") == 0) {
            cfg->animation_speed = (float)atof(val);
            if (cfg->animation_speed <= 0.01f) cfg->animation_speed = 0.05f;
            if (cfg->animation_speed > 1.0f) cfg->animation_speed = 1.0f;
        } else if (strcmp(key, "terminal") == 0) {
            snprintf(cfg->terminal, sizeof(cfg->terminal), "%s", val);
        }
    }

    fclose(f);
}
