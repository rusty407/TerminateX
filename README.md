# TerminateX

A modern Wayland compositor written in **C++23** using **wlroots 0.18** — the same library
powering Hyprland and Sway. Features a clean scene-graph architecture with floating window
management, full keyboard/pointer support, and nested-session development mode.

---

## Features

- **Auto-detecting backend** — runs nested inside Wayland/X11 sessions for development;
  switches to DRM/KMS automatically from a TTY
- **wlr_scene compositor** — damage-tracked, hardware-accelerated compositing with zero
  manual OpenGL code
- **XDG Shell** — full `xdg_toplevel` surface lifecycle (map, unmap, destroy, move, resize)
- **Floating layout** — click to focus, `Super+Drag` to move, resize handles from clients
- **Keybind table** — compile-time `keybinds.hpp` dispatch with modifier+keysym matching
- **Multi-keyboard** — all keyboards aggregated; repeat rate configurable
- **Cursor** — xcursor theme support, pointer enter/leave, axis (scroll)

## Keybindings

| Binding | Action |
|---|---|
| `Super + Enter` | Launch terminal (foot → alacritty → kitty → xterm) |
| `Super + Q` | Close focused window |
| `Super + F` | Toggle fullscreen |
| `Super + Escape` | Quit compositor |

---

## Dependencies

```bash
# Arch Linux
sudo pacman -S wlroots wayland wayland-protocols xkbcommon pixman cmake ninja gcc
```

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C build
```

## Run

```bash
# Nested (inside existing Wayland or X11 session) — default for development
./build/terminatex

# With a startup command
./build/terminatex foot

# From TTY (uses DRM/KMS — requires no DISPLAY/WAYLAND_DISPLAY set)
./build/terminatex
```

## Project Structure

```
TerminateX/
├── CMakeLists.txt        — CMake 3.25+ build, C++23, pkg-config wlroots
└── src/
    ├── main.cpp          — Entrypoint, startup command, logging
    ├── server.hpp/.cpp   — Central TXServer: owns all wlroots objects
    ├── output.hpp/.cpp   — Monitor lifecycle, scene-graph frame commits
    ├── view.hpp/.cpp     — XDG toplevel, focus, cascading placement
    ├── input.hpp/.cpp    — Keyboard, pointer, cursor grab, keybind dispatch
    └── keybinds.hpp      — Keybinding table (edit here to add shortcuts)
```
