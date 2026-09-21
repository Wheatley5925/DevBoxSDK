#pragma once

#include <string>

namespace devbox_linux_input {
// Load once: built-in bindings, then global settings, then game overrides.
void loadConfiguration();
bool usesGameConfiguration();
bool saveConfiguration(bool forThisGameOnly, std::string& error);
}
