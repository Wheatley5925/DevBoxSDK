#include "DevBoxTarget.h"

#if DEVBOX_TARGET == DEVBOX_TARGET_LINUX

#include "input.h"
#include <SDL2/SDL.h>
#include "input_keys.h"
#include "input_config.h"
#include "linux_ui.h"
#include <array>
#include <chrono>

namespace devbox_linux_input {
SDL_Scancode keys[buttonCount] = {
  SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
  SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
  SDL_SCANCODE_Z, SDL_SCANCODE_X,
  SDL_SCANCODE_A, SDL_SCANCODE_S,
  SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_RETURN
};
}

namespace {
using Clock = std::chrono::steady_clock;

struct ButtonState {
  bool stableReleased = true;
  bool lastReadingReleased = true;
  Clock::time_point lastChange = {};
};

std::array<ButtonState, devbox_linux_input::buttonCount> buttons;
bool initialized = false;
std::array<bool, SDL_NUM_SCANCODES> suppressed{};

bool pressEdge(ButtonState& button, bool released, Clock::time_point now) {
  if (released != button.lastReadingReleased) button.lastChange = now;
  button.lastReadingReleased = released;

  if (released != button.stableReleased &&
      now - button.lastChange >= std::chrono::milliseconds(50)) {
    const bool pressed = button.stableReleased && !released;
    button.stableReleased = released;
    return pressed;
  }
  return false;
}
}

void initButtons() {
  devbox_linux_input::loadConfiguration();
  SDL_PumpEvents();
  const auto now = Clock::now();
  for (ButtonState& button : buttons) {
    button = {true, true, now};
  }
  initialized = true;
}

void devbox_linux_input::suppressHeldKeys() {
  initButtons();
  const Uint8* state = SDL_GetKeyboardState(nullptr);
  for (int i = 0; i < SDL_NUM_SCANCODES; ++i) suppressed[i] = state[i] != 0;
}

bool buttonRaw(int index) {
  if (index < 0 || index >= devbox_linux_input::buttonCount) return false;
  devbox_linux_input::loadConfiguration();

  SDL_PumpEvents();
  const Uint8* state = SDL_GetKeyboardState(nullptr);
  const SDL_Scancode key = devbox_linux_input::keys[index];
  if (!state[key]) suppressed[key] = false;
  if (devboxLinuxControlsOpen() || suppressed[key]) return true;
  return state[key] == 0; // true means released
}

bool buttonPressed(int index) {
  if (index < 0 || index >= devbox_linux_input::buttonCount) return false;
  if (!initialized) initButtons();
  return pressEdge(buttons[index], buttonRaw(index), Clock::now());
}

#endif // DEVBOX_TARGET_LINUX
