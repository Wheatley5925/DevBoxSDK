#include "input.h"
#include <Arduino.h>
#include <ConsolePins.h>

const uint8_t BUTTON_PINS[] = 
{ 
  PIN_INPUT_UP,
  PIN_INPUT_DOWN,
  PIN_INPUT_LEFT,
  PIN_INPUT_RIGHT,
  PIN_INPUT_A,
  PIN_INPUT_B,  
  PIN_INPUT_X,
  PIN_INPUT_Y,
  PIN_INPUT_SEL,
  PIN_INPUT_START,
};

const int NUM_BUTTONS = sizeof(BUTTON_PINS);
const unsigned long DEBOUNCE_DELAY = 50;

#pragma once
struct Button {
    uint8_t pin;
    uint8_t state;
    uint8_t last_state;
    unsigned long last_debounce_time;
};

Button buttons[NUM_BUTTONS];

void initButtons() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i] = {BUTTON_PINS[i], HIGH, HIGH, 0};
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }
}

bool buttonPressed(int index) {
  if (index < 0 || index >= NUM_BUTTONS) return false;
  Button &button = buttons[index];

  uint8_t reading = digitalRead(button.pin);
  bool pressed = false;

  if (reading != button.last_state) {
    button.last_debounce_time = millis();
  }

  if ((millis() - button.last_debounce_time) > DEBOUNCE_DELAY) {
    if (button.state == HIGH && reading == LOW)
      pressed = true;
    button.state = reading;
  }

  button.last_state = reading;
  return pressed;
}

bool buttonRaw(int index) {
  if (index < 0 || index >= NUM_BUTTONS) return false;
  Button &button = buttons[index];

  return digitalRead(button.pin) == HIGH ? true : false;
}
