#pragma once

#include <cstddef>

using DevBoxTaskFunction = void (*)(void* argument);

bool devboxStartTask(DevBoxTaskFunction function,
                     void* argument,
                     const char* name,
                     size_t stackSize,
                     unsigned priority,
                     int core = -1);
