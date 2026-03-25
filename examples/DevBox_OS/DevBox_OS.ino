
#include "menu_app.h"
extern void app_menu();
sdmmc_card_t* g_sdcard = nullptr;

void setup() {
  Serial.begin(115200);    
  Serial.println("AAA");
  //old_display::initDisplay();
  //old_display::drawBox(0, 0, 256, 128);
  //old_display::sendToDisplay();
  //delay(1000);
  //softReboot();
  delay(100);
  initSD();
  initAudio();
  initDisplay();
  initButtons();
  //initDisplay();
  app_menu();
}

void loop() {
    
}
