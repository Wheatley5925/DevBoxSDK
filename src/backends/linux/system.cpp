#include "DevBoxTarget.h"

#if DEVBOX_TARGET_IS_DESKTOP

#include "DevBoxSystem.h"
#include "linux_ui.h"
#include <SDL2/SDL.h>
#include <atomic>
#include <cstdlib>

namespace {
std::atomic<DevBoxReturnBehavior> behavior{DevBoxReturnBehavior::CloseApplication};
}

void setDevBoxReturnBehavior(DevBoxReturnBehavior newBehavior) {
  behavior.store(newBehavior);
}

void devboxReturnToOS() {
  if (behavior.load() == DevBoxReturnBehavior::CloseApplication) {
    std::exit(EXIT_SUCCESS);
  }
}

bool devboxApplicationShouldClose() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) return true;
    devboxLinuxHandleUiEvent(event);
  }
  return false;
}

bool devboxApplicationPaused() {
  return devboxLinuxControlsOpen();
}

#endif // DEVBOX_TARGET_IS_DESKTOP
