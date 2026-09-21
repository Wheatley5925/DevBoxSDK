#include "display.h"
#include "backends/linux/input_keys.h"
#include "backends/linux/linux_ui.h"
#include <SDL2/SDL.h>
#include <u8g2.h>
#include <cstdio>

namespace {
const char* devBoxButtonName(SDL_Scancode key) {
  for (int i = 0; i < devbox_linux_input::buttonCount; ++i) {
    if (devbox_linux_input::keys[i] == key) return devbox_linux_input::names[i];
  }
  return nullptr;
}
}

int main() {
  if (!initDisplay()) return 1;

  // The left edge is black (0); the right edge is white (15).
  for (int y = 0; y < 128; ++y) {
    for (int x = 0; x < 256; x += 2) {
      const uint8_t left = static_cast<uint8_t>(x / 16);
      const uint8_t right = static_cast<uint8_t>((x + 1) / 16);
      fb4[y * 128 + x / 2] = static_cast<uint8_t>(left | (right << 4));
    }
  }

  setDisplayFont(u8g2_font_ncenB14_tr);
  drawText(8, 27, "DevBox", 15);

  setDisplayFont(u8g2_font_squeezed_b7_tr);
  drawBox(0, 42, 256, 20, 0);
  drawText(8, 55, "Last key: none", 15);

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      devboxLinuxHandleUiEvent(event);
      if (event.type == SDL_QUIT) running = false;
      if (!devboxLinuxControlsOpen() && event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        const char* button = devBoxButtonName(event.key.keysym.scancode);
        const char* key = SDL_GetKeyName(event.key.keysym.sym);
        char label[64];
        if (button) {
          std::snprintf(label, sizeof(label), "Last: %s (%s)", button, key);
        } else {
          std::snprintf(label, sizeof(label), "Last: keyboard %s", key);
        }
        drawBox(0, 42, 256, 20, 0);
        drawText(8, 55, label, 15);
      }
    }
    if (running) sendToDisplay();
    SDL_Delay(16);
  }
  return 0;
}
