cmake_minimum_required(VERSION 3.25)
project(TerminateX VERSION 0.1.0 LANGUAGES C CXX)

# ── Language standards ────────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── pkg-config dependencies ───────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)

pkg_check_modules(WLR     REQUIRED wlroots-0.21)
pkg_check_modules(WL_SRV  REQUIRED wayland-server)
pkg_check_modules(XKB     REQUIRED xkbcommon)
pkg_check_modules(PIXMAN  REQUIRED pixman-1)

# ── Sources ───────────────────────────────────────────────────────────────────
# wlroots 0.19+ uses C99 [static N] array params which are illegal in C++.
# GCC 16 rejects this even inside extern "C" {} blocks.
# Fix: compile all wlroots-touching TUs with LANGUAGE C (C11).
# Only main.cpp which calls no wlroots headers stays as C++23.
set(SOURCES_C
    src/server.cpp
    src/output.cpp
    src/view.cpp
    src/input.cpp
    src/tx_config.c
)

set(SOURCES_CXX
    src/main.cpp
)

set_source_files_properties(${SOURCES_C} PROPERTIES LANGUAGE C)

add_executable(terminatex ${SOURCES_C} ${SOURCES_CXX})

# ── Include dirs ──────────────────────────────────────────────────────────────
target_include_directories(terminatex PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${WLR_INCLUDE_DIRS}
    ${WL_SRV_INCLUDE_DIRS}
    ${XKB_INCLUDE_DIRS}
    ${PIXMAN_INCLUDE_DIRS}
)

# ── Link dirs + libs ──────────────────────────────────────────────────────────
target_link_directories(terminatex PRIVATE
    ${WLR_LIBRARY_DIRS}
    ${WL_SRV_LIBRARY_DIRS}
)

target_link_libraries(terminatex PRIVATE
    ${WLR_LIBRARIES}
    ${WL_SRV_LIBRARIES}
    ${XKB_LIBRARIES}
    ${PIXMAN_LIBRARIES}
)

# ── Compile flags: shared ─────────────────────────────────────────────────────
target_compile_options(terminatex PRIVATE
    -Wall -Wextra
    -Wno-unused-parameter
    $<$<CONFIG:Debug>:-g3 -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# ── WLR_USE_UNSTABLE + _GNU_SOURCE ────────────────────────────────────────────
# Applied to C TUs only (already #defined in each .cpp file, but belt+braces)
target_compile_definitions(terminatex PRIVATE
    WLR_USE_UNSTABLE
    _GNU_SOURCE
)

message(STATUS "wlroots version : ${WLR_VERSION}")
message(STATUS "wayland-server  : ${WL_SRV_VERSION}")
message(STATUS "xkbcommon       : ${XKB_VERSION}")
