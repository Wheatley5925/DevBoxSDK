#include "DevBoxTarget.h"

#if DEVBOX_TARGET == DEVBOX_TARGET_LINUX

#include "sd.h"
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {
std::string sdRoot;
}

bool initSD() {
  const char* configured = std::getenv("DEVBOX_SD_ROOT");
  const char* root = (configured && configured[0]) ? configured : "./sdcard";

  std::error_code error;
  const std::filesystem::path canonicalRoot = std::filesystem::canonical(root, error);
  if (error || !std::filesystem::is_directory(canonicalRoot, error) || error) {
    sdRoot.clear();
    return false;
  }

  sdRoot = canonicalRoot.string();
  return true;
}

std::string sdPath(const char* path) {
  if (!path) return {};
  const std::string input(path);
  if (input != "/sdcard" && input.rfind("/sdcard/", 0) != 0) return input;
  if (sdRoot.empty()) return {};
  return sdRoot + input.substr(7);
}

#endif // DEVBOX_TARGET_LINUX
