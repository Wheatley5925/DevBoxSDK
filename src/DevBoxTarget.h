#pragma once

// Choose one target in the build settings. The existing ESP32 board is the
// default so sketches that do not set DEVBOX_TARGET continue to work.
#define DEVBOX_TARGET_ESP32 1
#define DEVBOX_TARGET_LINUX 2
#define DEVBOX_TARGET_ESP32_RP2354 3
#define DEVBOX_TARGET_WINDOWS 4

#ifndef DEVBOX_TARGET
#define DEVBOX_TARGET DEVBOX_TARGET_ESP32
#endif

#if DEVBOX_TARGET != DEVBOX_TARGET_ESP32 && \
    DEVBOX_TARGET != DEVBOX_TARGET_LINUX && \
    DEVBOX_TARGET != DEVBOX_TARGET_ESP32_RP2354 && \
    DEVBOX_TARGET != DEVBOX_TARGET_WINDOWS
#error "Unknown DEVBOX_TARGET"
#endif

#define DEVBOX_TARGET_IS_DESKTOP \
    (DEVBOX_TARGET == DEVBOX_TARGET_LINUX || \
     DEVBOX_TARGET == DEVBOX_TARGET_WINDOWS)
