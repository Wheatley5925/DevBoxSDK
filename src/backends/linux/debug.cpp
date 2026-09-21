#include "DevBoxTarget.h"

#if DEVBOX_TARGET == DEVBOX_TARGET_LINUX

#include "DevBoxDebug.h"
#include <cstdarg>
#include <cstdio>

void initDebugOutput() {}

void devboxPrint(const char* text) {
  if (!text) return;
  std::fputs(text, stdout);
  std::fflush(stdout);
}

void devboxPrintf(const char* format, ...) {
  if (!format) return;
  va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stdout, format, arguments);
  va_end(arguments);
  std::fflush(stdout);
}

#endif // DEVBOX_TARGET_LINUX
