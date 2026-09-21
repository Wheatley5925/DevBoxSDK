#include "DevBoxTarget.h"

#if DEVBOX_TARGET == DEVBOX_TARGET_LINUX

#include "audio.h"
#include <SDL2/SDL.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
SDL_AudioDeviceID device = 0;
bool audioInitialized = false;
std::atomic<uint8_t> volume{255};

void closeAudio() {
  if (device) {
    SDL_CloseAudioDevice(device);
    device = 0;
  }
  if (audioInitialized) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    audioInitialized = false;
  }
}
}

bool initAudio() {
  if (device) return true;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    std::fprintf(stderr, "SDL audio initialization failed: %s\n", SDL_GetError());
    return false;
  }
  audioInitialized = true;

  SDL_AudioSpec wanted = {};
  wanted.freq = 44100;
  wanted.format = AUDIO_S16SYS;
  wanted.channels = 2;
  wanted.samples = 1024;
  device = SDL_OpenAudioDevice(nullptr, 0, &wanted, nullptr, 0);
  if (!device) {
    std::fprintf(stderr, "SDL audio device failed: %s\n", SDL_GetError());
    closeAudio();
    return false;
  }

  SDL_PauseAudioDevice(device, 0);
  std::atexit(closeAudio);
  return true;
}

void setAudioPaused(bool paused) {
  if (device) SDL_PauseAudioDevice(device, paused ? 1 : 0);
}

void setAudioVolume(uint8_t value) { volume.store(value); }

uint8_t getAudioVolume() { return volume.load(); }

void updateAudioVolumeFromPot() {}

int16_t applyVolumeToSample(int16_t sample) {
  return static_cast<int16_t>(static_cast<int32_t>(sample) * volume.load() / 255);
}

void applyVolumeToBuffer(int16_t* samples, size_t count) {
  const uint8_t level = volume.load();
  for (size_t i = 0; i < count; ++i) {
    samples[i] = static_cast<int16_t>(static_cast<int32_t>(samples[i]) * level / 255);
  }
}

size_t writeAudio(const void* data, size_t byteCount) {
  if (!device || (!data && byteCount)) return 0;

  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  size_t written = 0;
  while (written < byteCount) {
    while (SDL_GetQueuedAudioSize(device) >= 8192) SDL_Delay(1);

    const size_t remaining = byteCount - written;
    const Uint32 chunk = static_cast<Uint32>(remaining < 4096 ? remaining : 4096);
    if (SDL_QueueAudio(device, bytes + written, chunk) != 0) return written;
    written += chunk;
  }
  return written;
}

#endif // DEVBOX_TARGET_LINUX
