#include <driver/i2s.h>
#include "audio.h"

bool initAudio() {
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
  err = i2s_set_pin(I2S_NUM_0, &pins);

  return true;
}
