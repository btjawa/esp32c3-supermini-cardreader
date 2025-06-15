#pragma once

#include <Arduino.h>

// Global Settings

#define VERSION   0.3
#define BLUETOOTH_DEBUG

#define BOARD_LED 8
#define MAIN_LED  10

#define DIPSW1_IN 2
#define DIPSW2_IN 1

extern unsigned long ledUpdate;
extern unsigned long sysUpdate;
extern uint8_t hue;
extern uint8_t color[3];

extern uint8_t dipsw1; // segatools / spice2x
extern uint8_t dipsw2; // HW 115200 38400 / 1P 2P

// WS2812B LED

#include <FastLED.h>
#define BOARD_LED_LEN 1
#define MAIN_LED_LEN  8

extern CRGB main_leds[MAIN_LED_LEN];
extern CRGB board_led[BOARD_LED_LEN];

// SSD1306 OLED Display

#include <U8g2lib.h>
#include <Wire.h>

#define OLED_SDA 4
#define OLED_SCL 3

extern U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2;

void draw_blank(uint8_t x, uint8_t y);
void draw_icon(uint8_t x, uint8_t y, uint8_t addr);

// PN532 NFC/RFID Module

#include <Adafruit_PN532.h>
#include <SPI.h>

#define PN532_SCK  7
#define PN532_MISO 5
#define PN532_MOSI 6
#define PN532_SS   9

#define NFC_TIMEOUT 100

extern Adafruit_PN532 nfc;
extern uint32_t nfc_version;

// board/sg-cmd.h
struct sg_header {
	uint8_t frame_len;
	uint8_t addr;
	uint8_t seq_no;
	uint8_t cmd;
};

struct __attribute__((packed)) sg_res_header {
	struct sg_header hdr;
	uint8_t status;
	uint8_t payload_len;
	uint8_t payload[256];
};

struct __attribute__((packed)) sg_req_header {
	struct sg_header hdr;
	uint8_t pos;
	uint8_t checksum;
	uint8_t payload_len;
	uint8_t payload[256];
};