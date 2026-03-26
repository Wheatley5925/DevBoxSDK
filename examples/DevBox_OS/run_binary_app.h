#pragma once
#include <DevBoxSDK.h>
#include "bitmaps.h"
// run_binary_app.h
// will return false if the operation was unsuccessful or if successful perform esp_restart().
bool run_binary_app(const char* pathOnSD);
