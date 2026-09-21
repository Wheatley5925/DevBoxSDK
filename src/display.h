#pragma once
#include <cstdint>

extern uint8_t fb4[256 * 128 / 2]; // 16384 bytes

bool initDisplay();

// Call after initDisplay(). Pass a U8g2 font such as u8g2_font_ncenB14_tr.
void setDisplayFont(const uint8_t* font);

void clearGray(int gray);

// Clear the grayscale framebuffer to black.
void clearScreen();

void sendToDisplay();

// data: 4bpp packed, row-major, width=w, height=h
// byte layout per row: w/2 bytes (rounded up if odd)
void drawGrayBitmap(int x, int y, const uint8_t* data, int w, int h);

void drawBitmap(int x, int y, const uint8_t* data, int w, int h, int gray = 15, bool opaque = false, int bg = 0);

void drawText(int x, int y, const char* text, int gray = 15, bool opaque = false, int bg = 0);

void drawBox(int x, int y, int w, int h, int gray = 15);
