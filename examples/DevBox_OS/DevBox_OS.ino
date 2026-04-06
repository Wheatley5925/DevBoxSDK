
#include "menu_app.h"
extern void app_menu();

void setup() {
  Serial.begin(115200);    
  initSD();
  initAudio();
  initDisplay();
  initButtons();
  app_menu();
}

void loop() {
    
}
