#include "DevBoxTarget.h"

#if DEVBOX_TARGET == DEVBOX_TARGET_LINUX

#include "input_config.h"
#include "input_keys.h"
#include "detail/picojson.h" // PicoJSON v1.3.0, license retained in the header.
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unistd.h>
#include <vector>

#ifndef DEVBOX_GAME_ID
#define DEVBOX_GAME_ID ""
#endif

namespace devbox_linux_input {
namespace {
namespace fs = std::filesystem;
using Object = picojson::object;
using Bindings = std::array<SDL_Scancode, buttonCount>;
Bindings defaults{};
bool loaded = false;
bool gameConfiguration = false;
fs::path configDirectory;

// Encode unsafe filename bytes, including '%', so IDs cannot escape games/.
std::string gameFilename() {
  std::string result;
  for (unsigned char c : std::string(DEVBOX_GAME_ID)) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '-') {
      result += c;
    } else {
      char encoded[4];
      std::snprintf(encoded, sizeof(encoded), "%%%02X", c);
      result += encoded;
    }
  }
  return result.empty() ? result : result + ".json";
}

bool readSettings(const fs::path& path, Object& settings, std::string& error) {
  std::error_code ec;
  const bool exists = fs::exists(path, ec);
  if (ec) {
    error = path.string() + ": " + ec.message();
    return false;
  }
  if (!exists) return true;
  if (!fs::is_regular_file(path, ec) || ec) {
    error = "Expected a settings file at " + path.string();
    return false;
  }
  std::ifstream file(path);
  if (!file) {
    error = "Cannot read " + path.string();
    return false;
  }
  const std::string contents((std::istreambuf_iterator<char>(file)), {});
  picojson::value document;
  auto position = picojson::parse(document, contents.begin(), contents.end(), &error);
  while (position != contents.end() &&
         (*position == ' ' || *position == '\n' || *position == '\r' || *position == '\t')) ++position;
  if (file.bad() || !error.empty() || position != contents.end() || !document.is<Object>()) {
    error = "Invalid settings JSON in " + path.string();
    return false;
  }
  settings = document.get<Object>();
  const auto controls = settings.find("controls");
  if (controls == settings.end()) return true;
  if (!controls->second.is<Object>()) {
    error = "Expected a controls object in " + path.string();
    return false;
  }
  for (const char* name : names) {
    const auto& bindings = controls->second.get<Object>();
    const auto binding = bindings.find(name);
    if (binding == bindings.end()) continue;
    if (!binding->second.is<std::string>() ||
        SDL_GetScancodeFromName(binding->second.get<std::string>().c_str()) == SDL_SCANCODE_UNKNOWN) {
      error = "Invalid binding for " + std::string(name) + " in " + path.string();
      return false;
    }
  }
  return true;
}

void applySettings(const Object& settings, SDL_Scancode* bindings) {
  const auto controls = settings.find("controls");
  if (controls == settings.end()) return;
  const auto& values = controls->second.get<Object>();
  for (int i = 0; i < buttonCount; ++i) {
    const auto value = values.find(names[i]);
    if (value != values.end()) {
      bindings[i] = SDL_GetScancodeFromName(value->second.get<std::string>().c_str());
    }
  }
}

// Write beside the destination and rename: readers see a complete JSON file.
bool writeSettings(const fs::path& path, const Object& settings, std::string& error) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (ec) {
    error = path.string() + ": " + ec.message();
    return false;
  }
  const std::string pattern = path.string() + ".tmp-XXXXXX";
  std::vector<char> temporary(pattern.begin(), pattern.end());
  temporary.push_back('\0');
  const int fd = mkstemp(temporary.data()); // Private file, mode 0600.
  if (fd < 0) {
    error = path.string() + ": " + std::strerror(errno);
    return false;
  }
  const std::string contents = picojson::value(settings).serialize(true) + "\n";
  size_t offset = 0;
  int failure = 0;
  while (offset < contents.size()) {
    const ssize_t count = write(fd, contents.data() + offset, contents.size() - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) { failure = count < 0 ? errno : EIO; break; }
    offset += static_cast<size_t>(count);
  }
  if (!failure && fsync(fd) != 0) failure = errno;
  if (close(fd) != 0 && !failure) failure = errno;
  if (!failure && std::rename(temporary.data(), path.c_str()) != 0) failure = errno;
  if (failure) {
    std::remove(temporary.data());
    error = path.string() + ": " + std::strerror(failure);
    return false;
  }
  return true;
}
}

void loadConfiguration() {
  if (loaded) return;
  loaded = true;
  std::copy_n(keys, buttonCount, defaults.begin());
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  const char* home = std::getenv("HOME");
  if (xdg && fs::path(xdg).is_absolute()) configDirectory = fs::path(xdg) / "DevBox";
  else if (home && fs::path(home).is_absolute()) configDirectory = fs::path(home) / ".config" / "DevBox";
  else {
    std::fprintf(stderr, "DevBox settings: HOME or an absolute XDG_CONFIG_HOME is required\n");
    return;
  }

  Object global;
  std::string error;
  if (readSettings(configDirectory / "settings.json", global, error)) applySettings(global, keys);
  else std::fprintf(stderr, "DevBox settings: %s\n", error.c_str());
  if (gameFilename().empty()) return;
  Object game;
  error.clear();
  if (readSettings(configDirectory / "games" / gameFilename(), game, error)) {
    applySettings(game, keys);
    gameConfiguration = game.count("controls") != 0;
  } else std::fprintf(stderr, "DevBox settings: %s\n", error.c_str());
}

bool usesGameConfiguration() {
  loadConfiguration();
  return gameConfiguration;
}

bool saveConfiguration(bool forThisGameOnly, std::string& error) {
  loadConfiguration();
  error.clear();
  if (configDirectory.empty()) {
    error = "HOME or an absolute XDG_CONFIG_HOME is required";
    return false;
  }
  if (forThisGameOnly && gameFilename().empty()) {
    error = "This executable was built without DEVBOX_GAME_ID";
    return false;
  }
  const fs::path globalPath = configDirectory / "settings.json";
  const fs::path gamePath = configDirectory / "games" / gameFilename();
  Object global, game;
  // Read both before writing; never overwrite malformed settings.
  if (!readSettings(globalPath, global, error)) return false;
  if (!gameFilename().empty() && !readSettings(gamePath, game, error)) return false;
  Object& destination = forThisGameOnly ? game : global;
  Object controls;
  if (destination.count("controls")) controls = destination["controls"].get<Object>();
  Bindings globalBindings = defaults;
  applySettings(global, globalBindings.data());
  for (int i = 0; i < buttonCount; ++i) {
    const char* keyName = SDL_GetScancodeName(keys[i]);
    if (!keyName[0]) {
      error = "Cannot save unnamed keyboard key for " + std::string(names[i]);
      return false;
    }
    controls.erase(names[i]);
    if (!forThisGameOnly || keys[i] != globalBindings[i]) controls[names[i]] = picojson::value(std::string(keyName));
  }
  destination["controls"] = picojson::value(controls);
  if (!writeSettings(forThisGameOnly ? gamePath : globalPath, destination, error)) return false;
  if (!forThisGameOnly && game.erase("controls")) {
    if (!writeSettings(gamePath, game, error)) {
      error = "Global settings saved, but game override could not be removed: " + error;
      return false;
    }
  }
  gameConfiguration = forThisGameOnly;
  return true;
}
}

#endif // DEVBOX_TARGET_LINUX
