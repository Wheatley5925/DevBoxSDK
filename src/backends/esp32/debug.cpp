#include "DevBoxTarget.h"

#if DEVBOX_TARGET == DEVBOX_TARGET_ESP32

#include "DevBoxDebug.h"
#include <Arduino.h>
#include <cstdarg>
#include <cstdio>

void initDebugOutput() {
  Serial.begin(115200);
}

void devboxPrint(const char* text) {
  if (text) Serial.print(text);
}

void devboxPrintf(const char* format, ...) {
  if (!format) return;
  char buffer[256];
  va_list arguments;
  va_start(arguments, format);
  std::vsnprintf(buffer, sizeof(buffer), format, arguments);
  va_end(arguments);
  Serial.print(buffer);
}

#endif // DEVBOX_TARGET_ESP32
