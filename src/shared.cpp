#include "shared.h"

// Global Settings

unsigned long ledUpdate = 0;
unsigned long sysUpdate = 0;
uint8_t hue = 0;
uint8_t color[3] = {0, 0, 0};

uint8_t dipsw1 = 0;
uint8_t dipsw2 = 0;

// WS2812B LED

CRGB main_leds[MAIN_LED_LEN];
CRGB board_led[BOARD_LED_LEN];

// SSD1306 OLED Display

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void draw_blank(uint8_t x, uint8_t y) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y - 16, 16, 16);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void draw_icon(uint8_t x, uint8_t y, uint8_t addr) {
  u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
  u8g2.drawGlyph(x, y, addr);
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.sendBuffer();
}

// PN532 NFC/RFID Module

Adafruit_PN532 nfc(PN532_SS);

uint32_t nfc_version = 0;