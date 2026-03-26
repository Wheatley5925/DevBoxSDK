#include <Arduino.h>
#include <stdio.h>      // fopen, fread, fseek, ftell, fclose
#include <stdint.h>
#include <string>
#include <vector>

#include "run_binary_app.h"

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"




static void drawProgress(size_t done, size_t total)
{
    if (drawBox || sendToDisplay || total == 0) return;

    const int barX = 6;
    const int barY = 109;
    const int barW = 231;  // подгони под ширину экрана
    const int barH = 13;

    int filled = (int)((uint64_t)done * barW / total);

    drawBitmap(barX, barY, loading_bar_bitmap, barW, barH, 10, true, 0);
    drawBox(barX + filled, barY, barW - filled, barH, 0); // заполненная часть

    sendToDisplay();
}

#define MAX_LINE_LENGTH 256
static bool get_meta_data(std::vector<std::string>& meta, const char* pathOnSD)
{
    // Concatenate path cleanly
    std::string fullPath = std::string(pathOnSD) + ".meta";
    FILE* meta_file = fopen(fullPath.c_str(), "r");
    
    if (!meta_file) return false;
    
    char buffer[MAX_LINE_LENGTH];
    while(fgets(buffer, sizeof(buffer), meta_file)) 
    {
        std::string line = buffer;
        
        // Remove newline and carriage return (common in files)
        line.erase(line.find_last_not_of("\r\n") + 1);

        if(!line.empty()) {
            meta.push_back(line); // Automatically copies the data safely
        }
    }

    while (meta.size() < 3)
        meta.push_back("-");

    fclose(meta_file);
    return true;
}


bool run_binary_app(const char* pathOnSD)
{
    Serial.printf("[run_binary_app] fopen path='%s'\n", pathOnSD);
    clearGray(0);
    drawBitmap(0, 0, loading_screen_bitmap, 256, 128, 15, true, 0);
    sendToDisplay();

    // 1. Открываем файл через стандартный VFS
    FILE* fp = fopen(pathOnSD, "rb");
    if (!fp) {
        Serial.println("[run_binary_app] fopen failed");
        drawText(5, 75, "ERROR: Cannot open file.", 15);  sendToDisplay();
        delay(1500);
        return false;
    }

    // 2. Узнаём размер файла
    if (fseek(fp, 0, SEEK_END) != 0) {
        Serial.println("[run_binary_app] fseek end failed");
        drawText(5, 75, "ERROR: Seek failed.", 15);  sendToDisplay();
        fclose(fp);
        delay(1500);
        return false;
    }
    long fileSize = ftell(fp);
    if (fileSize < 0) {
        Serial.println("[run_binary_app] ftell failed");
        drawText(5, 75, "ERROR: Ftell failed.", 15);  sendToDisplay();             
        fclose(fp);
        delay(1500);
        return false;
    }
    rewind(fp);

    Serial.printf("[run_binary_app] fileSize=%ld bytes\n", fileSize);
    

    std::vector<std::string> meta_data{};
    if (get_meta_data(meta_data, pathOnSD)) {
        drawText(31, 39, meta_data[0].c_str(), 15);  
        drawText(49, 48, meta_data[1].c_str(), 10);
        drawText(39, 57, meta_data[2].c_str(), 10);
        drawText(42, 66, std::to_string(fileSize).c_str(), 10);
        sendToDisplay(); 
    }

    drawProgress(0, (size_t)fileSize);

    // 3. Находим OTA-раздел
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(nullptr);
    if (!update_partition) {
        Serial.println("[run_binary_app] no update partition");
        drawText(5, 75, "ERROR: No update partition.", 15);  sendToDisplay();
        fclose(fp);
        delay(1500);
        return false;
    }

    Serial.printf("[run_binary_app] writing to partition '%s' at 0x%08x, size=0x%08x\n",
                  update_partition->label,
                  update_partition->address,
                  update_partition->size);

    // 4. Начинаем OTA
    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        Serial.printf("[run_binary_app] esp_ota_begin failed: 0x%x\n", err);
        drawText(5, 75, "ERROR: esp_ota_begin failed.", 15);  sendToDisplay();
        fclose(fp);
        delay(1500);
        return false;
    }

    // 5. Читаем и пишем по кускам
    static const size_t BUF_SIZE = 4096;
    uint8_t* buf = (uint8_t*)malloc(BUF_SIZE);
    if (!buf) {
        drawText(5, 75, "ERROR: No memory.", 15);  sendToDisplay();
        fclose(fp);
        esp_ota_end(ota_handle);
        delay(1500);
        return false;
    }

    size_t totalWritten = 0;
    bool ok = true;

    while (true) {
        size_t n = fread(buf, 1, BUF_SIZE, fp);
        if (n == 0) {
            if (feof(fp)) {
                // EOF
                break;
            } else {
                Serial.println("[run_binary_app] fread error");
                drawText(5, 75, "ERROR: Fread error.", 15);  sendToDisplay();
                ok = false;
                break;
            }
        }

        err = esp_ota_write(ota_handle, buf, n);
        if (err != ESP_OK) {
            Serial.printf("[run_binary_app] esp_ota_write failed: 0x%x\n", err);
            ok = false;
            break;
        }

        totalWritten += n;
        drawProgress(totalWritten, (size_t)fileSize);
    }

    free(buf);
    fclose(fp);

    Serial.printf("[run_binary_app] totalWritten=%u bytes\n", (unsigned)totalWritten);

    if (!ok) {
        drawText(5, 75, "ERROR: Write failed.", 15);  sendToDisplay();
        esp_ota_end(ota_handle);
        delay(1500);
        return false;
    }

    // 6. Завершаем OTA
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        Serial.printf("[run_binary_app] esp_ota_end failed: 0x%x\n", err);
        drawText(5, 75, "ERROR: esp_ota_end failed.", 15);  sendToDisplay(); 
        delay(1500);
        return false;
    }

    // 7. Делаем раздел загрузочным
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        Serial.printf("[run_binary_app] esp_ota_set_boot_partition failed: 0x%x\n", err);
        drawText(5, 75, "ERROR: set_boot failed.", 15);  sendToDisplay();
        delay(1500);
        return false;
    }

    Serial.println("[run_binary_app] success, rebooting...");
    drawText(5, 75, "Now rebooting...", 15);  sendToDisplay();
    delay(500);

    esp_restart();
    while (true) { }

    return true;
}
