#include "sd.h"
#include <cstdio>
#include <string>

int main() {
  if (!initSD()) {
    std::fprintf(stderr, "SD folder not found (set DEVBOX_SD_ROOT)\n");
    return 1;
  }

  const std::string path = sdPath("/sdcard/hello.txt");
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (!file) {
    std::fprintf(stderr, "Cannot open %s\n", path.c_str());
    return 1;
  }

  char line[128];
  const bool read = std::fgets(line, sizeof(line), file) != nullptr;
  std::fclose(file);
  if (!read) return 1;

  std::printf("Read from virtual SD: %s", line);
  return 0;
}
