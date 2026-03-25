#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "audio_play.h"
#include "display.h"

struct AVConfig {
  const char* framesPath;   // "/sdcard/clip/frames.bin"
  const char* audioPath;    // "/sdcard/clip/audio.wav"
  float fps;                // e.g. 20.0
  int frameCount;           // 0 = auto from file size
};

bool avStart(const AVConfig& cfg);
void avStop();
bool avIsPlaying();

