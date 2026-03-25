// audio_play.cpp
#include <Arduino.h>
#include <stdio.h>
#include <driver/i2s.h>
#include "esp_timer.h"
#include "audio_play.h"

// Audio clock globals (used by video task)
int64_t  g_audio_start_us = 0;
uint64_t g_audio_bytes_written = 0;

uint32_t AUDIO_RATE = 44100;
uint32_t AUDIO_CH   = 2;
uint32_t AUDIO_BPS  = 16;
uint32_t BYTES_PER_SEC = AUDIO_RATE * AUDIO_CH * (AUDIO_BPS / 8); // 176400

bool playWav(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    return false;
  }

  fseek(f, 44, SEEK_SET);

  const size_t bufSize = 4096;
  static uint8_t buf[bufSize];

  bool started = false;
  g_audio_start_us = 0;
  g_audio_bytes_written = 0;

  while (!feof(f)) {
    size_t bytesRead = fread(buf, 1, bufSize, f);
    if (bytesRead == 0) break;

    size_t off = 0;
    while (off < bytesRead) {
      size_t written = 0;
      i2s_write(I2S_NUM_0, buf + off, bytesRead - off, &written, portMAX_DELAY);
      if (written == 0) break;

      if (!started) {
        g_audio_start_us = esp_timer_get_time();
        started = true;
      }

      off += written;
      g_audio_bytes_written += written;
    }
  }

  fclose(f);
  return true;
}
