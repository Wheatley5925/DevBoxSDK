#include "display.h"
#include <U8g2lib.h>
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include <ConsolePins.h>
#include <string>
extern "C" uint8_t u8x8_d_ssd1363_256x128(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

static const uint8_t data_pins[] = 
  {PIN_LCD_D0,
   PIN_LCD_D1,
   PIN_LCD_D2, 
   PIN_LCD_D3, 
   PIN_LCD_D4, 
   PIN_LCD_D5, 
   PIN_LCD_D6, 
   PIN_LCD_D7};

#define WR PIN_LCD_WR
#define DC PIN_LCD_DC
#define CS PIN_LCD_CS
#define RESET PIN_LCD_RST

U8G2 u8g2;

uint8_t fb4[256*128/2];

static uint32_t dmask_low[256];   // For GPIO 0-31
static uint32_t dmask_high[256];  // For GPIO 32-47
static uint32_t full_mask_low = 0;
static uint32_t full_mask_high = 0;
static bool table_ready = false;
static volatile bool ssd1363_in_ram_write = false;

static volatile bool g_ssd1363_safe_init = false;

using Gray4 = uint8_t;          // 0..15

static volatile Gray4 g_fg = 0x0F;   // white
static volatile Gray4 g_bg = 0x00;   // black

static Gray4 clamp4(int v) {
  return (v < 0) ? 0 : (v > 15) ? 15 : (Gray4)v;
}

static uint32_t fbIndexByte(int x, int y) {
  // x: 0..255, y: 0..127
  return (uint32_t)y * 128u + (uint32_t)(x >> 1); // 256/2 = 128 bytes per row
}


static void fbSetPixel(int x, int y, Gray4 g) {
  if ((unsigned)x >= 256u || (unsigned)y >= 128u) return;
  g &= 0x0F;
  uint32_t idx = fbIndexByte(x, y);
  uint8_t b = fb4[idx];
  if ((x & 1) == 0) {         // even x -> low nibble
    b = (b & 0xF0) | g;
  } else {                    // odd x -> high nibble
    b = (b & 0x0F) | (g << 4);
  }
  fb4[idx] = b;
}

static Gray4 fbGetPixel(int x, int y) {
  uint32_t idx = fbIndexByte(x, y);
  uint8_t b = fb4[idx];
  return (x & 1) ? (b >> 4) : (b & 0x0F);
}

void clearGray(int gray) {
  uint8_t g = clamp4(gray);
  uint8_t v = (g << 4) | g;
  memset(fb4, v, sizeof(fb4));
}

static void beginMask() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);   // draw '1' bits into mask
}

static uint8_t get_u8g2_tile_w() {
  return u8g2_GetBufferTileWidth(u8g2.getU8g2());
}
static uint8_t get_u8g2_tile_h() {
  return u8g2_GetBufferTileHeight(u8g2.getU8g2());
}
static uint8_t* get_u8g2_buf() {
  return u8g2_GetBufferPtr(u8g2.getU8g2());
}

void blitU8g2MaskToGray(int fg_gray, int bg_gray = 0, bool transparent = true) {
  const Gray4 fg = clamp4(fg_gray);
  const Gray4 bg = clamp4(bg_gray);

  const uint8_t tile_w = get_u8g2_tile_w();
  const uint8_t tile_h = get_u8g2_tile_h();
  uint8_t* buf = get_u8g2_buf();

  // buffer layout: tiles in row-major order, each tile = 8 bytes (8 columns), column-major bits
  for (uint8_t ty = 0; ty < tile_h; ty++) {
    for (uint8_t tx = 0; tx < tile_w; tx++) {

      const uint8_t* tile = buf + ((ty * tile_w + tx) * 8);
      const int base_x = tx * 8;
      const int base_y = ty * 8;

      // For each pixel row inside tile
      for (uint8_t row = 0; row < 8; row++) {
        // Each of 8 bytes is one column
        for (uint8_t col = 0; col < 8; col++) {
          const uint8_t colByte = tile[col];
          const bool on = (colByte >> row) & 1;

          if (on) {
            fbSetPixel(base_x + col, base_y + row, fg);
          } else if (!transparent) {
            fbSetPixel(base_x + col, base_y + row, bg);
          }
        }
      }
    }
  }
}


static void endMaskToGray(int fg, bool transparent = true, int bg = 0) {
  blitU8g2MaskToGray(fg, bg, transparent);
}

static const uint8_t u8x8_d_ssd1363_256x128_4bit_init_seq[] = {
  U8X8_DLY(10),
  U8X8_START_TRANSFER(),             	
  
  U8X8_CA(0xfd, 0x12),            	/* Unlock */
  U8X8_C(0xae),		                /* Display OFF */
  
  U8X8_CA(0xab, 0x01),			    /* Enable Internal VDD Regulator */  
  U8X8_CA(0xad, 0x80),			    /* Use EXTERNAL IREF (as recommended) */

  U8X8_CA(0xb3, 0x30),			    /* Clock divide */  
  U8X8_CA(0xca, 127),			    /* Mux ratio */  
  U8X8_CA(0xa2, 0x20),			    /* Display offset */  
  U8X8_CA(0xa1, 0x00),			    /* Start line */  
  
  U8X8_CAA(0xa0, 0x32, 0x00),	    /* 4-bit Grayscale Mode + Horizontal Inc */  
  
  U8X8_CAA(0xb4, 0x32, 0x00c),	    /* Enhancement A */  
  U8X8_CA(0xc1, 0xff),			    /* Contrast 255 */  
  U8X8_CA(0xba, 0x03),			    /* Vp cap */    
  U8X8_C(0xb9),		                /* Linear grayscale LUT */

  U8X8_CA(0xb1, 0x74),			    /* Phase periods */  
  U8X8_CA(0xbb, 0x0c),			    /* Precharge voltage */    
  U8X8_CA(0xb6, 0xc8),			    /* Second precharge */  
  U8X8_CA(0xbe, 0x04),			    /* VCOMH */  
  
  U8X8_C(0xa6),		                /* Normal display */
  U8X8_C(0xa9),		                /* Exit partial mode */
  
  U8X8_DLY(2),
  U8X8_C(0xaf),                     /* DISPLAY ON (Final Step) */
  U8X8_DLY(2),
  
  U8X8_END_TRANSFER(),             	
  U8X8_END()             			
};

void init_gpio_masks() {
  full_mask_low = 0;
  full_mask_high = 0;

  for (int i = 0; i < 8; ++i) { 
    if (data_pins[i] < 32) {
      full_mask_low |= (1UL << data_pins[i]);
    } else {
      full_mask_high |= (1UL << (data_pins[i] - 32));
    }
  }

  for (int v = 0; v < 256; ++v) {
    uint32_t low = 0, high = 0;
    for (int b = 0; b < 8; ++b) {
      if (v & (1 << b)) {
        if (data_pins[b] < 32)
          low |= (1UL << data_pins[b]);
        else
          high |= (1UL << (data_pins[b] - 32));
      }
    }
    dmask_low[v] = low;
    dmask_high[v] = high;
  }

  table_ready = true;
}

uint8_t u8x8_byte_ssd1363_8080_fast(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  switch (msg) {

    case U8X8_MSG_BYTE_INIT:
      for (int i = 0; i < 8; ++i) gpio_set_direction((gpio_num_t)data_pins[i], GPIO_MODE_OUTPUT);
      gpio_set_direction((gpio_num_t)WR, GPIO_MODE_OUTPUT);
      gpio_set_direction((gpio_num_t)DC, GPIO_MODE_OUTPUT);
      gpio_set_direction((gpio_num_t)CS, GPIO_MODE_OUTPUT);
      gpio_set_level((gpio_num_t)CS, 1);
      if (!table_ready) init_gpio_masks();
      break;

case U8X8_MSG_BYTE_SET_DC:
  if (DC < 32) {
    if (arg_int) GPIO.out_w1ts = (1UL << DC);
    else         GPIO.out_w1tc = (1UL << DC);
  } else {
    if (arg_int) GPIO.out1_w1ts.val = (1UL << (DC - 32));
    else         GPIO.out1_w1tc.val = (1UL << (DC - 32));
  }
  break;

    case U8X8_MSG_BYTE_START_TRANSFER:
      gpio_set_level((gpio_num_t)CS, 0);
      break;

    case U8X8_MSG_BYTE_END_TRANSFER:
      gpio_set_level((gpio_num_t)CS, 1);
      break;

case U8X8_MSG_BYTE_SEND: {
  uint8_t *ptr = (uint8_t *)arg_ptr;
  for (uint16_t i = 0; i < arg_int; i++) {
    uint8_t p = ptr[i];

    GPIO.out_w1tc      = full_mask_low  & ~dmask_low[p];
    GPIO.out1_w1tc.val = full_mask_high & ~dmask_high[p];
    GPIO.out_w1ts      = dmask_low[p];
    GPIO.out1_w1ts.val = dmask_high[p];

gpio_set_level((gpio_num_t)WR, 0);
        gpio_set_level((gpio_num_t)WR, 1);
  }
  break;
}
}
  return 1;
}

uint8_t u8x8_gpio_and_delay_esp32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  switch (msg) {
    case U8X8_MSG_DELAY_MILLI:
      delay(arg_int);
      break;
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      gpio_set_direction((gpio_num_t)RESET, GPIO_MODE_OUTPUT);
      break;
    case U8X8_MSG_GPIO_RESET:
      gpio_set_level((gpio_num_t)RESET, arg_int);
      break;
    default:
      return 0;
  }
  return 1;
}

// 1. Move the info struct to a global scope and ensure it's not local to a function
static const u8x8_display_info_t u8x8_ssd1363_256x128_4bit_info = {
  /* chip_reset_closed_level = */ 0,
  /* chip_precision = */ 1,
  /* post_chip_reset_wait_ms = */ 5,
  /* sda_setup_time_ns = */ 20,
  /* sclock_pulse_width_ns = */ 20, 
  /* tile_width = */ 32, 
  /* tile_hight = */ 16,
  /* default_x_offset = */ 0,
  /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 0, // 256
  /* pixel_height = */ 128
};

uint8_t u8x8_d_ssd1363_custom_callback(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  switch(msg) {
    case U8X8_MSG_DISPLAY_SETUP_MEMORY: {
      uint8_t r = u8x8_d_ssd1363_256x128(u8x8, msg, arg_int, arg_ptr);
      return r;
    }

    case U8X8_MSG_DISPLAY_INIT:
      u8x8->gpio_and_delay_cb(u8x8, U8X8_MSG_GPIO_AND_DELAY_INIT, 0, NULL);
      u8x8->byte_cb(u8x8, U8X8_MSG_BYTE_INIT, 0, NULL);

      // Let panel power rails settle after cold boot
      delay(120);

      // Strong hardware reset
      u8x8->gpio_and_delay_cb(u8x8, U8X8_MSG_GPIO_RESET, 0, NULL);
      delay(50);
      u8x8->gpio_and_delay_cb(u8x8, U8X8_MSG_GPIO_RESET, 1, NULL);
      delay(120);

      g_ssd1363_safe_init = true;
      u8x8_cad_SendSequence(u8x8, u8x8_d_ssd1363_256x128_4bit_init_seq);
      g_ssd1363_safe_init = false;

      // Optional final state commands, but keep them minimal
      u8x8_cad_StartTransfer(u8x8);
      u8x8_cad_SendCmd(u8x8, 0xA4); // display from RAM
      u8x8_cad_SendCmd(u8x8, 0xA6); // normal display
      u8x8_cad_EndTransfer(u8x8);

      break;

    case U8X8_MSG_DISPLAY_DRAW_TILE: {
      u8x8_tile_t *tile = (u8x8_tile_t *)arg_ptr;
      const uint8_t x = tile->x_pos;   // tile x (8px units)
      const uint8_t y = tile->y_pos;   // tile y (8px units)
      const uint8_t n = tile->cnt;     // number of tiles horizontally

      static uint8_t linebuf[4 * 32];  // max n=32 => 128 bytes

      u8x8_cad_StartTransfer(u8x8);

      const uint8_t col_off   = 0x08;            // GDDRAM col offset (0..79 space)
      const uint8_t col_start = col_off + x * 2; // 2 GDDRAM cols per tile (8px)
      const uint8_t col_end   = col_start + (2 * n) - 1;

      u8x8_cad_SendCmd(u8x8, 0x15);
      u8x8_cad_SendArg(u8x8, col_start);
      u8x8_cad_SendArg(u8x8, col_end);

      u8x8_cad_SendCmd(u8x8, 0x75);
      u8x8_cad_SendArg(u8x8, y * 8);
      u8x8_cad_SendArg(u8x8, (y * 8) + 7);

      u8x8_cad_SendCmd(u8x8, 0x5C);

      const int x_px = x * 8;           // pixel x
      const int bytes_per_line = 4 * n; // 8*n pixels /2 = 4*n bytes

      for (uint8_t row = 0; row < 8; row++) {
        const int y_px = (y * 8) + row;

        // copy contiguous packed bytes from framebuffer
        uint32_t src_idx = fbIndexByte(x_px, y_px);
        memcpy(linebuf, &fb4[src_idx], bytes_per_line);

        // keep your proven "swap pairs" quirk
        for (int i = 0; i < bytes_per_line; i += 2) {
          uint8_t tmp = linebuf[i];
          linebuf[i]  = linebuf[i + 1];
          linebuf[i + 1] = tmp;
        }

        u8x8_cad_SendData(u8x8, bytes_per_line, linebuf);
      }

      u8x8_cad_EndTransfer(u8x8);
      break;
    }

    default:
      return u8x8_d_ssd1363_256x128(u8x8, msg, arg_int, arg_ptr);
  }
  return 1;
}

void initDisplayForReboot() {
    // Raw hardware init — no u8g2, just wake the controller up
    gpio_set_direction((gpio_num_t)PIN_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_LCD_RST, 0);
    delay(50);
    gpio_set_level((gpio_num_t)PIN_LCD_RST, 1);
    delay(2000);  // let controller oscillator run for 2 seconds
    // No commands sent — just power cycling the reset line and waiting
}

bool initDisplay() { 
    init_gpio_masks(); // 1. Setup with the default driver FIRST to allocate memory 
    u8g2_Setup_ssd1363_256x128_f( u8g2.getU8g2(), U8G2_R0, u8x8_byte_ssd1363_8080_fast, u8x8_gpio_and_delay_esp32 ); // 2. ONLY hijack the display_cb, keep the rest of the u8g2 pointers intact 
    u8g2.begin();
    u8g2.getU8g2()->u8x8.display_cb = u8x8_d_ssd1363_custom_callback; 

    bool status = u8g2.begin(); 
    
    if (status) {
        u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_squeezed_b7_tr);
        u8g2.clearBuffer(); 
        u8g2.sendBuffer(); 
    }
    return status;
}

void setGray(int fg) { g_fg = clamp4(fg); }
void setGray(int fg, int bg) { g_fg = clamp4(fg); g_bg = clamp4(bg); }

void clearScreen() {
    u8g2.clearBuffer();
}

void sendToDisplay() {
    u8g2.sendBuffer();
}

// data: 4bpp packed, row-major, width=w, height=h
// byte layout per row: w/2 bytes (rounded up if odd)
void drawGrayBitmap(int x, int y, const uint8_t* data, int w, int h) {
  if (!data || w <= 0 || h <= 0) return;

  for (int yy = 0; yy < h; yy++) {
    int sy = y + yy;
    if ((unsigned)sy >= 128u) continue;

    const uint8_t* row = data + (yy * ((w + 1) >> 1));

    for (int xx = 0; xx < w; xx++) {
      int sx = x + xx;
      if ((unsigned)sx >= 256u) continue;

      uint8_t b = row[xx >> 1];
      Gray4 g = (xx & 1) ? (b >> 4) : (b & 0x0F);
      fbSetPixel(sx, sy, g);
    }
  }
}

void drawBitmap(int x, int y, const uint8_t* data, int w, int h, int gray, bool opaque, int bg) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);          // IMPORTANT: always 1 for mask
  u8g2.drawXBM(x, y, w, h, data);

  // stamp mask into fb4
  blitU8g2MaskToGray(gray, bg, /*transparent=*/!opaque);
}

void drawText(int x, int y, const char* text, int gray, bool opaque, int bg) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setCursor(x, y);
  u8g2.print(text);
  blitU8g2MaskToGray(gray, bg, /*transparent=*/!opaque);
}


void drawBox(int x, int y, int w, int h, int gray) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.drawBox(x, y, w, h);

  blitU8g2MaskToGray(gray, 0, /*transparent=*/true);
}

// New: set grayscale ink (0..15)
void setColor(int gray) {
  setGray(gray);
  u8g2.setDrawColor(1);  // keep u8g2 in normal draw mode
}

// Optional: if you still need u8g2 modes (0 clear, 1 set, 2 xor)
void setDrawMode(int mode01xor) {
  u8g2.setDrawColor(mode01xor);
}

