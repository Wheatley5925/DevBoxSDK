#include "DevBoxTarget.h"

#if DEVBOX_TARGET_IS_DESKTOP

#include "display.h"
#include "audio.h"
#include "input_keys.h"
#include "input_config.h"
#include "linux_ui.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <u8g2.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;
SDL_Texture* background = nullptr;
SDL_Texture* backgroundPressed = nullptr;
SDL_Texture* inputConfiguration = nullptr;
bool controlsOpen = false;
bool controlsForThisGameOnly = false;
std::array<SDL_Scancode, devbox_linux_input::buttonCount> originalKeys{};
bool originalScope = false;
bool controlsSaveFailed = false;
const SDL_Rect controlsSaveButton = {1, 107, 20, 20};
const SDL_Rect controlsRevertButton = {235, 107, 20, 20};
const SDL_Rect controlsScopeCheckbox = {79, 118, 9, 9};
int selectedButton = -1;
int swappedButton = -1;
bool waitingForKey = false;
// Coordinates are in the complete 268 x 144 window background.
const SDL_Rect volumeTrack = {137, 137, 35, 5};
constexpr int volumeKnobWidth = 3;
constexpr int volumeTravel = 32;
bool draggingVolume = false;
int volumeGrabOffset = 0;

int volumeKnobX() {
  return volumeTrack.x + (getAudioVolume() * volumeTravel + 127) / 255;
}

void moveVolumeKnob(int mouseX) {
  const int position = std::clamp(mouseX - volumeGrabOffset - volumeTrack.x,
                                  0, volumeTravel);
  setAudioVolume(static_cast<uint8_t>((position * 255 + volumeTravel / 2) / volumeTravel));
}

void stopVolumeDrag() {
  draggingVolume = false;
  SDL_CaptureMouse(SDL_FALSE);
}

bool handleVolumeEvent(const SDL_Event& event) {
  if (event.type == SDL_WINDOWEVENT &&
      event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
    if (draggingVolume) stopVolumeDrag();
    return false;
  }
  if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
    const SDL_Point point = {event.button.x, event.button.y};
    if (!SDL_PointInRect(&point, &volumeTrack)) return false;
    const int knobX = volumeKnobX();
    const bool onKnob = point.x >= knobX && point.x < knobX + volumeKnobWidth;
    // Preserve where the knob was grabbed; a track click centers it on the mouse.
    volumeGrabOffset = onKnob ? point.x - knobX : volumeKnobWidth / 2;
    draggingVolume = true;
    SDL_CaptureMouse(SDL_TRUE);
    if (!onKnob) moveVolumeKnob(point.x);
    return true;
  }
  if (draggingVolume && event.type == SDL_MOUSEMOTION) {
    if (!(event.motion.state & SDL_BUTTON_LMASK)) stopVolumeDrag();
    else moveVolumeKnob(event.motion.x);
    return true;
  }
  if (draggingVolume && event.type == SDL_MOUSEBUTTONUP &&
      event.button.button == SDL_BUTTON_LEFT) {
    moveVolumeKnob(event.button.x);
    stopVolumeDrag();
    return true; // Releasing over another toolbar button must not activate it.
  }
  return false;
}

void drawVolumeKnob() {
  // Cover the knob baked into the PNG, restoring the track underneath it.
  SDL_SetRenderDrawColor(renderer, 34, 34, 34, 255);
  SDL_RenderFillRect(renderer, &volumeTrack);
  const SDL_Rect trackInterior = {138, 138, 33, 3};
  SDL_SetRenderDrawColor(renderer, 51, 51, 51, 255);
  SDL_RenderFillRect(renderer, &trackInterior);
  const SDL_Rect knob = {volumeKnobX(), volumeTrack.y, volumeKnobWidth, volumeTrack.h};
  SDL_SetRenderDrawColor(renderer, 187, 187, 187, 255);
  SDL_RenderFillRect(renderer, &knob);
  const SDL_Rect highlight = {knob.x + 1, knob.y + 1, 1, 3};
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderFillRect(renderer, &highlight);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}

// Coordinates in InputConfiguration.png, in SDK button order.
const SDL_Rect buttonRects[] = {
  {41, 44, 14, 14}, // Up
  {41, 72, 14, 14}, // Down
  {27, 58, 14, 14}, // Left
  {55, 58, 14, 14}, // Right
  {201, 72, 14, 14}, // A
  {215, 58, 14, 14}, // B
  {187, 58, 14, 14}, // X
  {201, 44, 14, 14}, // Y
  {70, 99, 38, 14},  // Select
  {155, 99, 33, 14}  // Start
};
u8g2_t controlsMask = {};
bool videoInitialized = false;
bool imageInitialized = false;
u8g2_t mask = {};
bool maskInitialized = false;

void closeControls(bool save) {
  if (save) {
    std::string error;
    if (!devbox_linux_input::saveConfiguration(controlsForThisGameOnly, error)) {
      controlsSaveFailed = true;
      std::fprintf(stderr, "DevBox controls: %s\n", error.c_str());
      return;
    }
  } else {
    std::copy(originalKeys.begin(), originalKeys.end(), devbox_linux_input::keys);
    controlsForThisGameOnly = originalScope;
  }
  controlsOpen = false;
  waitingForKey = false;
  selectedButton = -1;
  swappedButton = -1;
  controlsSaveFailed = false;
  devbox_linux_input::suppressHeldKeys();
  setAudioPaused(false);
}

void openControls() {
  controlsForThisGameOnly = devbox_linux_input::usesGameConfiguration();
  std::copy_n(devbox_linux_input::keys, originalKeys.size(), originalKeys.begin());
  originalScope = controlsForThisGameOnly;
  controlsOpen = true;
  waitingForKey = false;
  selectedButton = -1;
  swappedButton = -1;
  controlsSaveFailed = false;
  devbox_linux_input::suppressHeldKeys();
  setAudioPaused(true);
}

void initMask() {
  if (maskInitialized) return;
  u8g2_Setup_ssd1363_256x128_f(&mask, U8G2_R0,
                                u8x8_byte_empty, u8x8_byte_empty);
  u8g2_SetFont(&mask, u8g2_font_squeezed_b7_tr);
  u8g2_ClearBuffer(&mask);
  maskInitialized = true;
}

void drawControlsText() {
  // A separate mask keeps the game's font and framebuffer intact.
  u8g2_ClearBuffer(&controlsMask);
  if (controlsSaveFailed) {
    u8g2_DrawStr(&controlsMask, 78, 43, "Save failed");
    u8g2_DrawStr(&controlsMask, 78, 55, "See terminal");
    u8g2_DrawStr(&controlsMask, 78, 74, "Retry or revert");
  } else if (selectedButton < 0) {
    u8g2_DrawStr(&controlsMask, 78, 43, "Click a button");
    u8g2_DrawStr(&controlsMask, 78, 55, "to change its key");
  } else {
    char line[21] = {};
    std::snprintf(line, sizeof(line), "DevBox: %s",
                  devbox_linux_input::names[selectedButton]);
    u8g2_DrawStr(&controlsMask, 78, 41, line);
    const char* keyName = SDL_GetScancodeName(
        devbox_linux_input::keys[selectedButton]);
    // Two short lines accommodate long keyboard names inside the 105px screen.
    const std::string keyboardLine = std::string("Keyboard: ") + keyName;
    std::snprintf(line, sizeof(line), "%.20s", keyboardLine.c_str());
    u8g2_DrawStr(&controlsMask, 78, 52, line);
    if (keyboardLine.size() > 20) {
      std::snprintf(line, sizeof(line), "%.20s", keyboardLine.c_str() + 20);
      u8g2_DrawStr(&controlsMask, 78, 62, line);
    }
    u8g2_DrawStr(&controlsMask, 78, 74,
                 waitingForKey ? "Press a key..." : "");
    if (!waitingForKey && swappedButton >= 0) {
      std::snprintf(line, sizeof(line), "Swapped with %s",
                    devbox_linux_input::names[swappedButton]);
      u8g2_DrawStr(&controlsMask, 78, 85, line);
    } else {
      u8g2_DrawStr(&controlsMask, 78, 85,
                   waitingForKey ? "Esc: cancel" : "Binding set");
    }
  }

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  const SDL_Rect textArea = {6 + 76, 6 + 32, 105, 58};
  SDL_RenderFillRect(renderer, &textArea);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  const uint8_t* bits = u8g2_GetBufferPtr(&controlsMask);
  const int tiles = u8g2_GetBufferTileWidth(&controlsMask);
  for (int y = 32; y < 90; ++y) {
    for (int x = 76; x < 181; ++x) {
      if (bits[((y / 8) * tiles + x / 8) * 8 + x % 8] & (1u << (y % 8))) {
        SDL_RenderDrawPoint(renderer, x + 6, y + 6);
      }
    }
  }
  if (selectedButton >= 0) {
    SDL_Rect highlight = buttonRects[selectedButton];
    highlight.x += 6;
    highlight.y += 6;
    SDL_RenderDrawRect(renderer, &highlight);
  }
  if (controlsForThisGameOnly) {
    const SDL_Rect checkMark = {6 + 81, 6 + 120, 5, 5};
    SDL_RenderFillRect(renderer, &checkMark);
  }
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}

void closeDisplay() {
  if (draggingVolume) stopVolumeDrag();
  SDL_DestroyTexture(texture);
  SDL_DestroyTexture(background);
  SDL_DestroyTexture(backgroundPressed);
  SDL_DestroyTexture(inputConfiguration);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  texture = nullptr;
  background = nullptr;
  backgroundPressed = nullptr;
  inputConfiguration = nullptr;
  renderer = nullptr;
  window = nullptr;

  if (imageInitialized) {
    IMG_Quit();
    imageInitialized = false;
  }
  if (videoInitialized) {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    videoInitialized = false;
  }
}

uint8_t clampGray(int gray) {
  if (gray < 0) return 0;
  if (gray > 15) return 15;
  return static_cast<uint8_t>(gray);
}

void setPixel(int x, int y, uint8_t gray) {
  if (x < 0 || x >= 256 || y < 0 || y >= 128) return;

  uint8_t& packed = fb4[y * 128 + x / 2];
  if (x & 1) {
    packed = static_cast<uint8_t>((packed & 0x0F) | (gray << 4));
  } else {
    packed = static_cast<uint8_t>((packed & 0xF0) | gray);
  }
}

void blitMask(int x0, int y0, int x1, int y1, uint8_t gray) {
  const uint8_t* bits = u8g2_GetBufferPtr(&mask);
  const int tilesPerRow = u8g2_GetBufferTileWidth(&mask);
  for (int y = std::max(y0, 0); y < std::min(y1, 128); ++y) {
    for (int x = std::max(x0, 0); x < std::min(x1, 256); ++x) {
      const int tile = (y / 8) * tilesPerRow + x / 8;
      if (bits[tile * 8 + x % 8] & (1u << (y % 8))) {
        setPixel(x, y, gray);
      }
    }
  }
}
}

void devboxLinuxHandleUiEvent(const SDL_Event& event) {
  if (!renderer) return;
  if (handleVolumeEvent(event)) return;
  if (controlsOpen && waitingForKey && event.type == SDL_KEYDOWN &&
      event.key.repeat == 0) {
    controlsSaveFailed = false;
    const SDL_Scancode key = event.key.keysym.scancode;
    if (key == SDL_SCANCODE_ESCAPE) {
      waitingForKey = false;
      selectedButton = -1;
      swappedButton = -1;
    } else if (key > SDL_SCANCODE_UNKNOWN && key < SDL_NUM_SCANCODES) {
      // Give the other button our old key before taking its binding.
      for (int i = 0; i < devbox_linux_input::buttonCount; ++i) {
        if (i != selectedButton && devbox_linux_input::keys[i] == key) {
          devbox_linux_input::keys[i] = devbox_linux_input::keys[selectedButton];
          swappedButton = i;
          break;
        }
      }
      devbox_linux_input::keys[selectedButton] = key;
      waitingForKey = false;
    }
    return;
  }
  if (event.type != SDL_MOUSEBUTTONUP ||
      event.button.button != SDL_BUTTON_LEFT) return;

  const int logicalX = event.button.x;
  const int logicalY = event.button.y;
  if (logicalX >= 253 && logicalX < 262 &&
      logicalY >= 135 && logicalY < 144) {
    // Let the application's event loop perform its normal window-close cleanup.
    SDL_Event quit = {};
    quit.type = SDL_QUIT;
    if (SDL_PushEvent(&quit) < 0) {
      std::fprintf(stderr, "DevBox power button: %s\n", SDL_GetError());
    }
    return;
  }
  if (logicalX >= 175 && logicalX < 214 &&
      logicalY >= 135 && logicalY < 144) {
    if (controlsOpen) closeControls(true);
    else openControls();
    return;
  }
  if (controlsOpen) {
    // SDL already supplies logical coordinates; remove only the screen offset.
    const SDL_Point point = {logicalX - 6, logicalY - 6};
    if (SDL_PointInRect(&point, &controlsSaveButton)) {
      closeControls(true);
      return;
    }
    if (SDL_PointInRect(&point, &controlsRevertButton)) {
      closeControls(false);
      return;
    }
    if (SDL_PointInRect(&point, &controlsScopeCheckbox)) {
      controlsForThisGameOnly = !controlsForThisGameOnly;
      controlsSaveFailed = false;
      return;
    }
    for (int i = 0; i < devbox_linux_input::buttonCount; ++i) {
      if (SDL_PointInRect(&point, &buttonRects[i])) {
        selectedButton = i;
        swappedButton = -1;
        waitingForKey = true;
        controlsSaveFailed = false;
        break;
      }
    }
  }
}

bool devboxLinuxControlsOpen() {
  return controlsOpen;
}

uint8_t fb4[256 * 128 / 2] = {};

bool initDisplay() {
  if (texture) return true;

  initMask();

  u8g2_Setup_ssd1363_256x128_f(&controlsMask, U8G2_R0,
                              u8x8_byte_empty, u8x8_byte_empty);
  u8g2_SetFont(&controlsMask, u8g2_font_5x7_tr);
  u8g2_SetDrawColor(&controlsMask, 1);

  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "SDL video initialization failed: %s\n", SDL_GetError());
    return false;
  }
  videoInitialized = true;
  devbox_linux_input::loadConfiguration();

  if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
    std::fprintf(stderr, "SDL PNG initialization failed: %s\n", IMG_GetError());
    closeDisplay();
    return false;
  }
  imageInitialized = true;

  SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  window = SDL_CreateWindow("DevBox", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 268 * 4, 144 * 4,
                            SDL_WINDOW_RESIZABLE);
  if (window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (renderer) texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, 256, 128);

  if (!texture) {
    std::fprintf(stderr, "SDL display creation failed: %s\n", SDL_GetError());
    closeDisplay();
    return false;
  }

  char* basePath = SDL_GetBasePath();
  const std::string assetRoot = basePath ? basePath : "";
  SDL_free(basePath);
  const std::string imagePath = assetRoot + "DevBoxPlayerBackground.png";
  const std::string configurationPath = assetRoot + "InputConfiguration.png";

  SDL_Surface* image = IMG_Load(imagePath.c_str());
  if (!image || image->w != 268 || image->h != 144) {
    std::fprintf(stderr, "Cannot load 268 x 144 background %s: %s\n",
                 imagePath.c_str(), IMG_GetError());
    SDL_FreeSurface(image);
    closeDisplay();
    return false;
  }
  background = SDL_CreateTextureFromSurface(renderer, image);

  SDL_Surface* pressed = SDL_ConvertSurfaceFormat(
      image, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(image);
  if (pressed) {
    for (int y = 135; y < 144; ++y) {
      auto* row = reinterpret_cast<uint32_t*>(
          static_cast<uint8_t*>(pressed->pixels) + y * pressed->pitch);
      for (int x = 175; x < 214; ++x) {
        uint8_t red, green, blue, alpha;
        SDL_GetRGBA(row[x], pressed->format, &red, &green, &blue, &alpha);
        row[x] = SDL_MapRGBA(pressed->format, 255 - red, 255 - green,
                             255 - blue, alpha);
      }
    }
    backgroundPressed = SDL_CreateTextureFromSurface(renderer, pressed);
    SDL_FreeSurface(pressed);
  }

  SDL_Surface* configuration = IMG_Load(configurationPath.c_str());
  if (configuration && configuration->w == 256 && configuration->h == 128) {
    inputConfiguration = SDL_CreateTextureFromSurface(renderer, configuration);
    if (inputConfiguration) {
      SDL_SetTextureBlendMode(inputConfiguration, SDL_BLENDMODE_BLEND);
    }
  }
  SDL_FreeSurface(configuration);

  if (!background || !backgroundPressed || !inputConfiguration ||
      SDL_RenderSetLogicalSize(renderer, 268, 144) != 0) {
    std::fprintf(stderr, "SDL simulator artwork setup failed: %s\n", SDL_GetError());
    closeDisplay();
    return false;
  }

  std::atexit(closeDisplay);
  return true;
}

void setDisplayFont(const uint8_t* font) {
  if (!font) return;
  initMask();
  u8g2_SetFont(&mask, font);
}

void clearGray(int gray) {
  const uint8_t shade = clampGray(gray);
  const uint8_t packed = static_cast<uint8_t>((shade << 4) | shade);
  std::memset(fb4, packed, sizeof(fb4));
}

void clearScreen() {
  clearGray(0);
}

void drawGrayBitmap(int x, int y, const uint8_t* data, int w, int h) {
  if (!data || w <= 0 || h <= 0) return;

  for (int yy = 0; yy < h; ++yy) {
    const int screenY = y + yy;
    if (screenY < 0 || screenY >= 128) continue;

    const uint8_t* row = data + yy * ((w + 1) / 2);
    for (int xx = 0; xx < w; ++xx) {
      const int screenX = x + xx;
      if (screenX < 0 || screenX >= 256) continue;

      const uint8_t source = row[xx / 2];
      const uint8_t gray = (xx & 1) ? (source >> 4) : (source & 0x0F);
      setPixel(screenX, screenY, gray);
    }
  }
}

void drawBox(int x, int y, int w, int h, int gray) {
  if (w <= 0 || h <= 0) return;

  const uint8_t shade = clampGray(gray);
  for (int yy = 0; yy < h; ++yy) {
    for (int xx = 0; xx < w; ++xx) {
      setPixel(x + xx, y + yy, shade);
    }
  }
}

void drawBitmap(int x, int y, const uint8_t* data, int w, int h,
                int gray, bool opaque, int bg) {
  if (!data || w <= 0 || h <= 0) return;

  const uint8_t foreground = clampGray(gray);
  const uint8_t background = clampGray(bg);
  const int bytesPerRow = (w + 7) / 8;

  for (int yy = 0; yy < h; ++yy) {
    for (int xx = 0; xx < w; ++xx) {
      const uint8_t bits = data[yy * bytesPerRow + xx / 8];
      const bool on = (bits & (1u << (xx & 7))) != 0;
      if (on || opaque) {
        setPixel(x + xx, y + yy, on ? foreground : background);
      }
    }
  }
}

void drawText(int x, int y, const char* text, int gray, bool opaque, int bg) {
  if (!text || x < 0 || y < 0) return;
  initMask();
  u8g2_ClearBuffer(&mask);
  u8g2_SetDrawColor(&mask, 1);
  u8g2_DrawStr(&mask, x, y, text);

  const int top = y - u8g2_GetAscent(&mask);
  const int bottom = y - u8g2_GetDescent(&mask);
  const int right = x + u8g2_GetStrWidth(&mask, text);
  if (opaque) drawBox(x, top, right - x, bottom - top, bg);
  blitMask(x, top, right, bottom, clampGray(gray));
}

void sendToDisplay() {
  if (!renderer || !texture) return;

  const SDL_Rect screen = {6, 6, 256, 128};
  if (controlsOpen) {
    if (SDL_RenderClear(renderer) != 0) return;
    if (SDL_RenderCopy(renderer, backgroundPressed, nullptr, nullptr) != 0) return;
    if (SDL_RenderCopy(renderer, texture, nullptr, &screen) != 0) return;
    if (SDL_RenderCopy(renderer, inputConfiguration, nullptr, &screen) != 0) return;
    drawControlsText();
    drawVolumeKnob();
    SDL_RenderPresent(renderer);
    return;
  }

  static uint32_t pixels[256 * 128];
  for (int y = 0; y < 128; ++y) {
    for (int x = 0; x < 256; ++x) {
      const uint8_t packed = fb4[y * 128 + x / 2];
      const uint8_t gray4 = (x & 1) ? (packed >> 4) : (packed & 0x0F);
      const uint32_t gray8 = gray4 * 17u;
      pixels[y * 256 + x] = 0xFF000000u | (gray8 << 16) |
                            (gray8 << 8) | gray8;
    }
  }

  if (SDL_UpdateTexture(texture, nullptr, pixels, 256 * sizeof(uint32_t)) != 0) return;
  if (SDL_RenderClear(renderer) != 0) return;
  if (SDL_RenderCopy(renderer, background, nullptr, nullptr) != 0) return;
  if (SDL_RenderCopy(renderer, texture, nullptr, &screen) != 0) return;
  drawVolumeKnob();
  SDL_RenderPresent(renderer);
}

#endif // DEVBOX_TARGET_IS_DESKTOP
