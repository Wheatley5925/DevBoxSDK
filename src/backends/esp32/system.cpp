#include "DevBoxTarget.h"

#if DEVBOX_TARGET == DEVBOX_TARGET_ESP32

#include "DevBoxSystem.h"
#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>

void setDevBoxReturnBehavior(DevBoxReturnBehavior) {}

void devboxReturnToOS() {
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (!next) {
    Serial.println("returnToOS: no next OTA partition");
    delay(100);
    esp_restart();
  }

  const esp_err_t error = esp_ota_set_boot_partition(next);
  if (error != ESP_OK) {
    Serial.printf("returnToOS: esp_ota_set_boot_partition failed: %d\n",
                  static_cast<int>(error));
    delay(100);
    esp_restart();
  }

  delay(50);
  esp_restart();
}

bool devboxApplicationShouldClose() {
  return false;
}

bool devboxApplicationPaused() {
  return false;
}

#endif // DEVBOX_TARGET_ESP32
