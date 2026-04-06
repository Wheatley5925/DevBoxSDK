#include "sd.h"

sdmmc_card_t* g_sdcard = nullptr;

bool initSD() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.flags = SDMMC_HOST_FLAG_1BIT;
  host.max_freq_khz = 5000;

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.clk = GPIO_NUM_39;
  slot_config.cmd = GPIO_NUM_40;
  slot_config.d0  = GPIO_NUM_38;

  gpio_set_pull_mode(GPIO_NUM_38, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(GPIO_NUM_39, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(GPIO_NUM_40, GPIO_PULLUP_ONLY);

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 4096
  };

  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &g_sdcard);

  if (ret != ESP_OK) {
    return false;
  }

  return true;
}
