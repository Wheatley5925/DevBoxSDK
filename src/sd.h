#pragma once

#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

extern sdmmc_card_t* g_sdcard;

bool initSD();
