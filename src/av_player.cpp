#include "av_player.h"
#include "display.h"
#include "audio_play.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <driver/i2s.h>
#include "esp_timer.h"

// ---- constants
constexpr int W = 256;
constexpr int H = 128;
constexpr int FRAME_BYTES = W * H / 2;

// Must match your I2S config (44100Hz, 16-bit, stereo)
constexpr uint32_t AUDIO_RATE = 44100;
constexpr uint32_t AUDIO_CH   = 2;
constexpr uint32_t AUDIO_BPS  = 16;
constexpr uint32_t BYTES_PER_SEC = AUDIO_RATE * AUDIO_CH * (AUDIO_BPS / 8);

// ---- state
static TaskHandle_t s_audioTask = nullptr;
static TaskHandle_t s_videoTask = nullptr;
static volatile bool s_playing = false;

static AVConfig s_cfg;

// audio clock derived from the audio task runtime
static volatile int64_t  s_audio_start_us = 0;
static volatile uint64_t s_audio_bytes_written = 0;

// ---- helpers
static inline void silenceI2S() {
  // Stops output quickly and avoids "stuck last sample"
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_stop(I2S_NUM_0);
}

static inline uint64_t fileSize(FILE* f) {
  long cur = ftell(f);
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, cur, SEEK_SET);
  return (sz < 0) ? 0 : (uint64_t)sz;
}

// ---- audio task
inline static void audioTask(void*) {
  // We want bytes_written clock, but playWav doesn't report it.
  // So we do a local WAV streaming loop here (same style as yours),
  // but it remains "contained" in this module.
  FILE* f = fopen(s_cfg.audioPath, "rb");
  if (!f) {
    s_playing = false;
    vTaskDelete(nullptr);
  }

  // Skip WAV header (ffmpeg pcm_s16le usually OK)
  fseek(f, 44, SEEK_SET);

  const size_t BUF = 4096;
  static uint8_t buf[BUF];

  bool started = false;
  s_audio_start_us = 0;
  s_audio_bytes_written = 0;

  while (s_playing && !feof(f)) {
    size_t rd = fread(buf, 1, BUF, f);
    if (rd == 0) break;

    size_t written = 0;
    // finite timeout so stop works quickly
    i2s_write(I2S_NUM_0, buf, rd, &written, pdMS_TO_TICKS(20));

    if (written) {
      if (!started) {
        s_audio_start_us = esp_timer_get_time();
        started = true;
      }
      s_audio_bytes_written += written;
    }
  }

  fclose(f);
  silenceI2S();

  // If audio ends naturally, stop video too
  s_playing = false;
  vTaskDelete(nullptr);
}

// ---- video task


static void videoTask(void*) {
  FILE* f = fopen(s_cfg.framesPath, "rb");
  if (!f) {
    s_playing = false;
    vTaskDelete(nullptr);
  }

  // Big stdio buffering helps a LOT on FAT/SD
  static uint8_t fileBuf[64 * 1024];
  setvbuf(f, (char*)fileBuf, _IOFBF, sizeof(fileBuf));

  // totalFrames
  int totalFrames = s_cfg.frameCount;
  if (totalFrames <= 0) {
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    totalFrames = (sz > 0) ? (int)(sz / FRAME_BYTES) : 0;
  }
  if (totalFrames <= 0) {
    fclose(f);
    s_playing = false;
    vTaskDelete(nullptr);
  }

  // Wait until audio starts (or until stopped)
  while (s_playing && s_audio_start_us == 0) vTaskDelay(1);

  static uint8_t framebuf[FRAME_BYTES];

  int fileFrame = 0;     // next frame index in file position
  int shownFrame = -1;

  while (s_playing) {
    // audio time -> desired frame
    double t = (double)s_audio_bytes_written / (double)BYTES_PER_SEC;
    int want = (int)(t * (double)s_cfg.fps);

    if (want >= totalFrames) break;
    if (want < 0) want = 0;

    // If we're behind, skip frames by reading/discarding
    while (fileFrame < want) {
      size_t rd = fread(framebuf, 1, FRAME_BYTES, f);
      if (rd != FRAME_BYTES) { s_playing = false; break; }
      fileFrame++;
    }
    if (!s_playing) break;

    // Show exactly one frame when it's time
    if (fileFrame == want && shownFrame != want) {
      size_t rd = fread(framebuf, 1, FRAME_BYTES, f);
      if (rd != FRAME_BYTES) break;

      memcpy(fb4, framebuf, FRAME_BYTES);
      sendToDisplay();

      shownFrame = want;
      fileFrame++;
      continue;
    }

    // If we're already ahead (rare), just yield briefly
    vTaskDelay(1);
  }

  fclose(f);
  s_playing = false;
  vTaskDelete(nullptr);
}

// ---- public API
bool avStart(const AVConfig& cfg) {
  if (s_playing) return false;

  s_cfg = cfg;
  s_playing = true;

  // Ensure I2S is running (you should have initAudio() called already)
  // i2s_start(I2S_NUM_0);  // optional; driver usually starts on first write

  BaseType_t okA = xTaskCreatePinnedToCore(audioTask, "av_audio", 4096, nullptr, 3, &s_audioTask, 0);
  if (okA != pdPASS) { s_playing = false; return false; }

  BaseType_t okV = xTaskCreatePinnedToCore(videoTask, "av_video", 8192, nullptr, 2, &s_videoTask, 1);
  if (okV != pdPASS) {
    s_playing = false;
    vTaskDelete(s_audioTask);
    s_audioTask = nullptr;
    return false;
  }

  return true;
}

void avStop() {
  if (!s_playing) return;

  s_playing = false;

  // Kill tasks (fast stop, minimal invasiveness)
  if (s_videoTask) { vTaskDelete(s_videoTask); s_videoTask = nullptr; }
  if (s_audioTask) { vTaskDelete(s_audioTask); s_audioTask = nullptr; }

  silenceI2S();
}

bool avIsPlaying() {
  return s_playing;
}
