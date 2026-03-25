#pragma once
#include <DevBoxSDK.h>
// run_binary_app.h
// Возвращает true, если удалось прошить и (теоретически) перезагрузиться.
// По факту при успехе функция не вернётся, потому что будет esp_restart().
bool run_binary_app(const char* pathOnSD);
