#include <driver/i2s.h>
#include "audio.h"
#include "Arduino.h"

static uint8_t g_audioVolume = 255;
static uint16_t g_potFiltered = 0;  

bool initAudio() {
  pinMode(17, INPUT);

  analogReadResolution(12);   // 0..4095 on ESP32
  // optional:
  // analogSetPinAttenuation(PIN_AUD_POT, ADC_11db);

  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pins = {
    .bck_io_num = 2,
    .ws_io_num = 1,
    .data_out_num = 43,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  if (err != ESP_OK) return false;

  err = i2s_set_pin(I2S_NUM_0, &pins);
  if (err != ESP_OK) return false;

  updateAudioVolumeFromPot();
  return true;
}

void setAudioVolume(uint8_t v) {
  g_audioVolume = v;
}

uint8_t getAudioVolume() {
  return g_audioVolume;
}

void updateAudioVolumeFromPot() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; ++i) {
    sum += analogRead(17);
  }
  uint16_t raw = sum / 8;

  // simple smoothing
  g_potFiltered = (g_potFiltered * 3 + raw) / 4;

  // map 0..4095 -> 0..255
  uint32_t x = (uint32_t)g_potFiltered * 255 / 4095;

  // optional curve: better feel at low volume
  x = (x * x) / 255;

  g_audioVolume = (uint8_t)x;
}

int16_t applyVolumeToSample(int16_t s) {
  return (int32_t(s) * g_audioVolume) / 255;
}

void applyVolumeToBuffer(int16_t* samples, size_t count) {
  const uint8_t vol = g_audioVolume;
  for (size_t i = 0; i < count; ++i) {
    samples[i] = (int32_t(samples[i]) * vol) / 255;
  }
}
