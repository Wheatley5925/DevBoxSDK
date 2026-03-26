#include <DevBoxSDK.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "picture.h"

sdmmc_card_t* g_sdcard = nullptr;

void returnToOS() {
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    if (!next) {
        Serial.println("not next");
        return;
    }

    esp_err_t err = esp_ota_set_boot_partition(next);
    if (err != ESP_OK) {
        Serial.println("esp_ota_set_boot_partition = true");
        return;
    }

    esp_restart();
}

void setup() {
    Serial.begin(115200);
    Serial.println("=== TestApp started ===");

    Serial.println("initSD...");
    initSD();
    Serial.println("initSD OK");

    Serial.println("initAudio...");
    initAudio();
    Serial.println("initAudio OK");

    Serial.println("initDisplay...");
    bool ok = initDisplay();
    Serial.printf("initDisplay OK=%d\n", ok);

    Serial.println("initButtons...");
    initButtons();
    Serial.println("initButtons OK");
    Serial.println("Initiated");
    clearGray(0);

    // Sample screen data transfer speed test
    uint32_t t0 = micros();
    drawGrayBitmap(0, 0, picture_4bpp, picture_w, picture_h);
    uint32_t dt = micros() - t0;
    Serial.printf("sendToDisplay: %u us, %.1f FPS\n", dt, 1e6f / dt);
}

void loop() {
    // SELECT + START to return to the previous OTA partition
    // (default: DevBox_OS app)
    if (buttonPressed(8) && buttonPressed(9)) {
        clearScreen();
        drawText(0, 10, "Exiting", 8);    
        sendToDisplay();

        Serial.println("returning to factory");
        returnToOS();
    }
    delay(1);
}
