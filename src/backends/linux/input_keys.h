#pragma once

#include <SDL2/SDL.h>

namespace devbox_linux_input {
constexpr int buttonCount = 10;

// Order matches the ESP32 BUTTON_PINS array.
extern SDL_Scancode keys[buttonCount];

void suppressHeldKeys();

constexpr const char* names[buttonCount] = {
  "Up", "Down", "Left", "Right", "A", "B", "X", "Y", "Select", "Start"
};
}
