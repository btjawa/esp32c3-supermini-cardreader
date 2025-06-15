#include "sega.h"
#include "spice.h"
#include "shared.h"

#include "esp_system.h"
#include "esp_heap_caps.h"

#ifdef BLUETOOTH_DEBUG
#include "bluetooth.h"
#endif

void setup(void) {
  pinMode(BOARD_LED, INPUT);
  pinMode(BOARD_LED, OUTPUT);
  pinMode(MAIN_LED,  OUTPUT);
  pinMode(DIPSW1_IN, INPUT_PULLUP);
  pinMode(DIPSW2_IN, INPUT_PULLUP);

  dipsw1 = digitalRead(DIPSW1_IN);
  dipsw2 = digitalRead(DIPSW2_IN);

  SPI.begin(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
  Wire.begin(OLED_SDA, OLED_SCL);
  FastLED.addLeds<WS2812B, MAIN_LED, GRB>(main_leds, MAIN_LED_LEN);
  FastLED.addLeds<WS2812, BOARD_LED, GRB>(board_led, BOARD_LED_LEN);
  FastLED.setBrightness(50);
  nfc.begin();
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.clearBuffer();

  digitalWrite(BOARD_LED, HIGH);

  u8g2.setCursor(0, 7);
  u8g2.printf("Aime Card Reader v%.1f", VERSION);
  nfc_version = nfc.getFirmwareVersion();
  if (!nfc_version) {
    u8g2.drawStr(0, 32, "NFC/RFID not found.");
  } else if (dipsw1) {
    Serial.begin(57600);
    u8g2.drawStr(98, 32, "SPICE");
    u8g2.drawStr(80, 32, dipsw2 ? "2P" : "1P");
  } else {
    Serial.begin(dipsw2 ? 38400 : 115200);
    u8g2.drawStr(104, 32, "SEGA");
    u8g2.drawStr(80, 32, dipsw2 ? "CVT" : "SP");
  }
  u8g2.sendBuffer();

  // https://www.nxp.com/docs/en/user-guide/141520.pdf
  nfc.setPassiveActivationRetries(0x0A);
  nfc.SAMConfig();

  #ifdef BLUETOOTH_DEBUG
  setup_bluetooth();
  #endif
}

void loop() {
  uint32_t now = millis();
  if (now - ledUpdate >= 50) {
    ledUpdate = now;
    CRGB displayColor;
    if (color[0] == 0 && color[1] == 0 && color[2] == 0) {
      displayColor = CHSV(hue++, 255, 255);
    } else {
      displayColor = CRGB(color[0], color[1], color[2]);
    }
    board_led[0] = displayColor;
    for (uint8_t i=0; i<MAIN_LED_LEN; i++) {
      main_leds[i] = displayColor;
    }
    FastLED.show();
  }
  if (now - sysUpdate >= 500) {
    sysUpdate = now;
    debugLog("Free heap size", esp_get_free_heap_size());
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);
    debugLog("Largest free block", heap_info.largest_free_block);
  }
  if (dipsw1) {
    spice::insert_card();
  } else {
    sega::handle_frame();
  }
}