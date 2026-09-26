#pragma once

#include "DevBoxTarget.h"
#include <string>

#if !DEVBOX_TARGET_IS_DESKTOP
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

extern sdmmc_card_t* g_sdcard;
#endif

bool initSD();

// Resolve a /sdcard path to the active storage root. Other paths are unchanged.
// On desktop targets, call initSD() first; an empty result means the root is unavailable.
std::string sdPath(const char* path);
