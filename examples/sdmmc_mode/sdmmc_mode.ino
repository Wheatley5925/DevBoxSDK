#include "sd.h"
#include "bitmaps.h"
#include <DevBoxSDK.h>
#include "USB.h"
#include "USBMSC.h"

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

sdmmc_card_t* g_sdcard = nullptr;   // реальное определение глобальной переменной

USBMSC msc;

static const uint32_t SECTOR_SIZE = 512;

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

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

// === Callbacks for USB MSC ===

// Чтение секторов с SD
int32_t my_msc_read_cb(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    if (!g_sdcard) return -1;

    uint32_t sector = lba + offset / SECTOR_SIZE;
    uint32_t count  = bufsize / SECTOR_SIZE;

    esp_err_t err = sdmmc_read_sectors(g_sdcard, buffer, sector, count);
    if (err != ESP_OK) {
        // Serial.printf("sdmmc_read_sectors err=0x%x\n", err);
        return -1;
    }
    return (int32_t)bufsize;
}

// Запись секторов на SD
int32_t my_msc_write_cb(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    if (!g_sdcard) return -1;

    uint32_t sector = lba + offset / SECTOR_SIZE;
    uint32_t count  = bufsize / SECTOR_SIZE;

    esp_err_t err = sdmmc_write_sectors(g_sdcard, buffer, sector, count);
    if (err != ESP_OK) {
        // Serial.printf("sdmmc_write_sectors err=0x%x\n", err);
        return -1;
    }
    return (int32_t)bufsize;
}

// Обработка start/stop (eject). Пока просто всегда ок.
bool my_msc_start_stop_cb(uint8_t power_condition, bool start, bool load_eject) {
    (void)power_condition;
    (void)start;
    (void)load_eject;
    return true;
}

void setup() {
    // В режиме MSC USB-Serial может работать странно, но для начала можно включить
    Serial.begin(115200);
    delay(1000);
    Serial.println("USB MSC SD mode starting...");  
    initButtons();
    initDisplay();
    clearScreen();
    drawBitmap(0, 0, usb_mode_bitmap, 256, 128);
    sendToDisplay();
    // 1. Монтируем SD тем же способом, как в DevBox_OS
    if (!initSD()) {
        drawText(0, 10, "SD init failed", 1);
        sendToDisplay();
        Serial.println("SD init failed");
        while (true) {
            delay(1000);
        }
    }

    // 2. Узнаём геометрию карты
    uint32_t block_size  = g_sdcard->csd.sector_size; // обычно 512
    uint32_t block_count = g_sdcard->csd.capacity;    // число секторов

    Serial.printf("SD: block_size=%u, block_count=%u\n", block_size, block_count);

    // 3. Настраиваем MSC
    msc.vendorID("DEVBOX");       // до 8 символов
    msc.productID("SDCARD");      // до 16 символов
    msc.productRevision("1.0");

    msc.onRead(my_msc_read_cb);
    msc.onWrite(my_msc_write_cb);
    msc.onStartStop(my_msc_start_stop_cb);

    // 4. Запускаем MSC
    msc.begin(block_count, block_size);
    msc.mediaPresent(true);

    // 5. Стартуем USB
    USB.begin();
    
    drawText(0, 10, "USB MSC started. SD card should appear on host.", 1);
    sendToDisplay();
    Serial.println("USB MSC started. SD card should appear on host.");
}

void loop() {
  if (buttonPressed(3)) {
    drawText(0, 10, "RETURNING...", 1);
    Serial.println("returning to factory");
    sendToDisplay();
    returnToOS();
  }
    // Потом сюда можно повесить кнопку "вернуться в OS" (через DevBoxSDK input)
    // if (isButtonJustPressed(...)) returnToOS();

    delay(10);
}
