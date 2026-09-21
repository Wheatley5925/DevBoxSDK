#pragma once

enum class DevBoxReturnBehavior {
  CloseApplication,
  Ignore
};

void setDevBoxReturnBehavior(DevBoxReturnBehavior behavior);
void devboxReturnToOS();
bool devboxApplicationShouldClose();
bool devboxApplicationPaused();
