#include "audio.h"
#include "display.h"
#include "input.h"
#include "backends/linux/linux_ui.h"
#include <SDL2/SDL.h>
#include <u8g2.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
void playTone() {
  constexpr int sampleRate = 44100;
  constexpr int frames = sampleRate / 5;
  constexpr double twoPi = 6.283185307179586;
  std::vector<int16_t> samples(frames * 2);

  for (int i = 0; i < frames; ++i) {
    const int fadeFrames = std::min(i, frames - 1 - i);
    const double fade = std::min(fadeFrames, 220) / 220.0;
    const int16_t value = static_cast<int16_t>(
        6000.0 * fade * std::sin(twoPi * 440.0 * i / sampleRate));
    samples[2 * i] = value;
    samples[2 * i + 1] = value;
  }

  applyVolumeToBuffer(samples.data(), samples.size());
  writeAudio(samples.data(), samples.size() * sizeof(int16_t));
}
}

int main() {
  if (!initDisplay() || !initAudio()) return 1;
  initButtons();

  clearGray(0);
  setDisplayFont(u8g2_font_squeezed_b7_tr);
  drawText(12, 45, "Press A (Z) for a tone", 15);
  drawText(12, 62, "Esc to quit", 15);
  sendToDisplay();

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      const bool configuringControls = devboxLinuxControlsOpen();
      devboxLinuxHandleUiEvent(event);
      if (event.type == SDL_QUIT ||
          (!configuringControls && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
        running = false;
      }
    }
    if (buttonPressed(4)) playTone();
    if (running) sendToDisplay();
    SDL_Delay(16);
  }
  return 0;
}
