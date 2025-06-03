#include <Arduino.h>
#include "main.h"

unsigned long lastUpdate;
uint8_t hue = 0;
uint8_t color[3] = {0, 0, 0};

void setup_bluetooth() {
  BLEDevice::init("ESP32C3_Debug");
  BLEServer *pServer = BLEDevice::createServer();
  class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
      pServer->getAdvertising()->start();
    }

    void onDisconnect(BLEServer* pServer) override {
      pServer->getAdvertising()->start();
    }
  };
  pServer->setCallbacks(new ServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pLogCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  BLEDescriptor *pDescriptor = new BLE2902();
  pLogCharacteristic->addDescriptor(pDescriptor);
  pLogCharacteristic->setValue("Ready");
  pService->start();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setMinInterval(0x20);
  pAdvertising->setMaxInterval(0x40);
  pAdvertising->start();
}

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
  digitalWrite(BOARD_LED, HIGH);
  nfc.begin();
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.clearBuffer();
  Serial.begin(115200);

  u8g2.setCursor(0, 7);
  u8g2.printf("Aime Card Reader v%.1f", VERSION);
  nfc_version = nfc.getFirmwareVersion();
  if (nfc_version) {
    u8g2.drawStr(dipsw1? 98: 104, 32, dipsw1? "SPICE": "SEGA");
    u8g2.drawStr(80, 32, dipsw1? (dipsw2 ? "2P" : "1P"): (dipsw2 ? "CVT" : "SP"));
  } else {
    u8g2.drawStr(0, 32, "NFC/RFID not found.");
  }
  u8g2.sendBuffer();

  // https://www.nxp.com/docs/en/user-guide/141520.pdf
  nfc.setPassiveActivationRetries(0x0A);
  nfc.SAMConfig();

  #ifdef BLUETOOTH_DEBUG
  setup_bluetooth();
  #endif
}

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

void loop() {
  uint32_t now = millis();
  if (now - lastUpdate >= 50) {
    lastUpdate = now;
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
  if (!dipsw1 && sg_read_frame()) {
    switch (req.hdr.cmd) {
      // Version
      case CMD_GET_FW_VERSION: {
        const version_info *fw = &fw_version[dipsw2 ? 1 : 2];
        sg_res_init(fw->length);
        memcpy(res.payload, fw->version, fw->length);
        break;
      }
      case CMD_GET_HW_VERSION: {
        const version_info *hw = &hw_version[dipsw2 ? 1 : 2];
        sg_res_init(hw->length);
        memcpy(res.payload, hw->version, hw->length);
        break;
      }

      // NFC/RFID
      // https://github.com/elechouse/PN532/blob/PN532_HSU/PN532/PN532.cpp
      case CMD_NFC_RADIO_ON: {
        uint8_t cmd[] = { 0x32, 0x01, 0x01 };
        sg_res_init((uint8_t)nfc.sendCommandCheckAck(cmd, sizeof(cmd), NFC_TIMEOUT));
        draw_icon(0,32,0x008B);
        break;
      }
      case CMD_NFC_RADIO_OFF: {
        // Check last response
        if (res.hdr.cmd == CMD_FELICA_ENCAP) {
          draw_icon(0, 32, 0x0073);
        } else {
          draw_blank(0, 32);
        }
        uint8_t cmd[] = { 0x32, 0x01, 0x00 };
        sg_res_init((uint8_t)nfc.sendCommandCheckAck(cmd, sizeof(cmd), NFC_TIMEOUT));
        break;
      }
      case CMD_NFC_POLL: {
        sg_res_init(1, 1);
        sg_nfc_poll_mifare mifare;
        sg_nfc_poll_felica felica;
        uint8_t luid[10];
        uint8_t id_len;
        uint8_t IDm[8];
        uint8_t PMm[8];
        uint16_t sys_code[4] = { 0 };
        draw_icon(0,32,0x008B);
        if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, luid, &id_len, NFC_TIMEOUT)) {
          uint8_t uid_len = sizeof(mifare.uid);
          mifare.type = 0x10;
          mifare.id_len = min(id_len, uid_len);
          mifare.count = 1;
          memset(mifare.uid, 0, uid_len);
          memcpy(mifare.uid, luid, mifare.id_len);
          sg_res_init(sizeof(mifare));
          memcpy(res.payload, &mifare, sizeof(mifare));
        } else if (nfc.felica_Polling(0xFFFF, 0x01, IDm, PMm, sys_code, NFC_TIMEOUT)) {
          uint8_t idm_len = sizeof(felica.IDm);
          uint8_t pmm_len = sizeof(felica.PMm);
          felica.type = 0x20;
          felica.id_len = idm_len + pmm_len;
          felica.count = 1;
          memcpy(felica.IDm, IDm, idm_len);
          memcpy(felica.PMm, PMm, pmm_len);
          sg_res_init(sizeof(felica));
          memcpy(res.payload, &felica, sizeof(felica));
        }
        break;
      }
      // Since UID will be sent by CMD_MIFARE_AUTHENTICATE_XXX, we dummy this cmd here
      case CMD_MIFARE_SELECT_TAG:
      // Unknown
      case CMD_MIFARE_UNKNOWN:
        sg_res_init();
        break;

      // Mifare
      case CMD_MIFARE_SET_KEY_AIME:
        sg_res_init();
        memcpy(aime_key, res.payload, sizeof(aime_key));
        break;
      case CMD_MIFARE_SET_KEY_BANA:
        sg_res_init();
        memcpy(bana_key, res.payload, sizeof(bana_key));
        break;
      // https://github.com/adafruit/Adafruit-PN532/blob/master/examples/readMifare/readMifare.ino
      case CMD_MIFARE_AUTHENTICATE_AIME:
      case CMD_MIFARE_AUTHENTICATE_BANA: {
        sg_res_init(0, 1);
        uint8_t uid[4];
        memcpy(uid, req.payload, sizeof(uid));
        uint8_t* key = req.hdr.cmd == 0x51 ? aime_key : bana_key;
        if (nfc.mifareclassic_AuthenticateBlock(uid, sizeof(uid), req.payload[4], 1, key)) {
          sg_res_init();
        }
        break;
      }
      case CMD_MIFARE_READ_BLOCK: {
        sg_res_init(0, 1);
        uint8_t uid[4];
        uint8_t data[16];
        memcpy(uid, req.payload, sizeof(uid));
        if (nfc.mifareclassic_ReadDataBlock(req.payload[4], data)) {
          sg_res_init(sizeof(data));
          memcpy(res.payload, data, sizeof(data));
        }
        break;
      }

      // Device control
      case CMD_TO_UPDATE_MODE: 
        sg_res_init();
        break;
      case CMD_SEND_HEX_DATA:
        sg_res_init();
        /* Firmware checksum length? */
        if (req.payload_len == 0x2b) {
          /* The firmware is identical flag? */
          res.status = 0x20;
        }
        break;
      case CMD_RESET:
        sg_res_init();
        res.status = 3;
        draw_blank(0, 32);
        break;

      // Felica
      case CMD_FELICA_ENCAP: {
        sg_res_init(0, 1);
        uint8_t *payload = &req.payload[8];
        uint8_t resp[256];
        uint8_t resp_len = 0xFF;
        if (payload[0] != req.payload_len - 8) {
          debugLog("FeliCa encap payload length mismatch", payload[0]);
          break;
        }
        if (nfc.inDataExchange(payload, payload[0], resp, &resp_len)) {
          sg_res_init(resp_len);
          memcpy(res.payload, &resp, sizeof(resp));
        }
        break;
      }

      // LED
      case CMD_RGB_SET_COLOR:
        sg_res_init();
        memcpy(color, req.payload, sizeof(color));
        break;
      case CMD_RGB_GET_INFO: {
        const version_info *fw = &led_version[dipsw2 ? 1 : 2];
        sg_res_init(fw->length);
        memcpy(res.payload, fw->version, fw->length);
        break;
      }
      case CMD_RGB_RESET:
        sg_res_init();
        memset(color, 0, sizeof(color));
        break;
    }
    debugBLEHeader();
    sg_write_frame();
  }
}