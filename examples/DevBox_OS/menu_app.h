#ifndef MENU_APP_H
#define MENU_APP_H

#include "run_binary_app.h"
#include <dirent.h>
#include <vector>
#include <string>

static bool exit_requested = false;

void requestExit() {
    exit_requested = true;
}

bool isExitRequested() {
    return exit_requested;
}




void app_menu () {
  Serial.println("Entering app_menu");

  DIR* dir = opendir("/sdcard/apps");
  if (!dir) {
    clearGray(0);
    Serial.println("No SD card present");
    drawText(0, 10, "No SD card present", 15);
    sendToDisplay();
    delay(1500);
    return;
  }
  

  drawBitmap(0, 0, logo_bitmap, 256, 128, 15, true, 0);
  sendToDisplay();
  playWav("/sdcard/sound/startup.wav");

std::vector<std::string> appEntries;
struct dirent* entry;

while ((entry = readdir(dir)) != NULL) {
    std::string name = entry->d_name;
    if (name == "." || name == "..") continue;

    // Old format: flat .bin files in /apps
    if (entry->d_type == DT_REG) {
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".bin") {
            appEntries.emplace_back(name);
        }
        continue;
    }

    // New format: packaged app directory in /apps/<name>/
    if (entry->d_type == DT_DIR) {
        std::string folderPath = std::string("/sdcard/apps/") + name;
        std::string fixedBin = folderPath + "/app.bin";
        std::string legacyBin = folderPath + "/" + name + ".bin";

        if (access(fixedBin.c_str(), F_OK) == 0) {
            appEntries.emplace_back(name + "/app.bin");
        } else if (access(legacyBin.c_str(), F_OK) == 0) {
            appEntries.emplace_back(name + "/" + name + ".bin");
        }
    }
}
closedir(dir);

  if (appEntries.empty()) {
    clearGray(0);
    drawText(0, 10, "No apps found", 15);
    sendToDisplay();
    delay(1500);
    return;
  }
  clearGray(0);
  drawBitmap(0, 0, app_select_bitmap, 256, 128, 15, true, 0); 
  
  int selected = 0;
  unsigned long lastDraw = 0;

  while (true) {
    if (millis() - lastDraw > 100) {
      drawBox(0, 24, 128, 92, 0);
      for (int i = 0; i < (int)appEntries.size(); ++i) {
        const int y = i * 12 + 34;
        if (i == selected) {
          drawBox(0, i * 12 + 24, 128, 12, 15);
          drawText(4, y, appEntries[i].c_str(), 0);
        } else {
          drawText(4, y, appEntries[i].c_str(), 15);
        }
      }

      sendToDisplay();
      lastDraw = millis();
    }

    // Down
    if (buttonPressed(1)) {
      selected = (selected + 1) % appEntries.size();
      delay(150); 
    }

    // Up 
    if (buttonPressed(0)) {
      selected = (selected - 1 + appEntries.size()) % appEntries.size();
      delay(150);
    }

    // A → run the selected app
    if (buttonPressed(3)) {
      const char* name = appEntries[selected].c_str();
      std::string full = "/sdcard/apps/" + std::string(name);

      if (strstr(name, ".bin")) {
        // call the corresponding function  
        run_binary_app(full.c_str());
        // the function will do esp_restart if successful
      break;
      }
    }
  }
}

#endif
