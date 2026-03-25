//#include <DevBoxSDK.h>
#include "ConsolePins.h"
#include "display.h"

#include "input.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "picture.h"

//sdmmc_card_t* g_sdcard = nullptr;

void returnToOS() {
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    if (!next) {
        Serial.println("not next");
        // Можно вывести что-то на экран или в Serial, но не обязательно
        return;
    }

    esp_err_t err = esp_ota_set_boot_partition(next);
    if (err != ESP_OK) {
        Serial.println("esp_ota_set_boot_partition = true");
        // Тоже можно залогировать ошибку
        return;
    }

    esp_restart();  // после этого загрузится DevBox_OS
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== TestApp started ===");

  Serial.println("initSD...");
  //initSD();
  Serial.println("initSD OK");

  Serial.println("initAudio...");
  //initAudio();
  Serial.println("initAudio OK");

  Serial.println("initDisplay...");
  bool ok = initDisplay();
  Serial.printf("initDisplay OK=%d\n", ok);

  Serial.println("initButtons...");
  initButtons();
  Serial.println("initButtons OK");
  Serial.println("Initiated");
  clearGray(0);
  //drawDiagnosticPattern();
  drawGrayBitmap(0, 0, picture_4bpp, picture_w, picture_h);

  uint32_t t0 = micros();
  uint32_t dt = micros() - t0;
  Serial.printf("sendToDisplay: %u us, %.1f FPS\n", dt, 1e6f / dt);
}
void loop() {
    if (buttonPressed(8) && buttonPressed(9)) {
        clearScreen();
        drawText(0, 10, "Exiting", 8);    
        sendToDisplay();

        Serial.println("returning to factory");
        returnToOS();
    }
    delay(1);
}
