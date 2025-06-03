// Global Settings

#define VERSION   0.3
#define BLUETOOTH_DEBUG

#define BOARD_LED 8
#define MAIN_LED  10

#define DIPSW1_IN 2
#define DIPSW2_IN 1

uint8_t dipsw1  = 0; // segatools / spicetools
uint8_t dipsw2  = 0; // HW 115200 38400 / PLAYER 1 2

// WS2812B LED

#include <FastLED.h>
#define BOARD_LED_LEN 1
#define MAIN_LED_LEN  8

CRGB main_leds[MAIN_LED_LEN];
CRGB board_led[BOARD_LED_LEN];

// SSD1306 OLED Display

#include <U8g2lib.h>
#include <Wire.h>

#define OLED_SDA 4
#define OLED_SCL 3

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// PN532 NFC/RFID Module

#include <Adafruit_PN532.h>
#include <SPI.h>

#define PN532_SCK  7
#define PN532_MISO 5
#define PN532_MOSI 6
#define PN532_SS   9

#define NFC_TIMEOUT 100

Adafruit_PN532 nfc(PN532_SS);

uint32_t nfc_version;
uint8_t  aime_key[6];
uint8_t  bana_key[6];

// LightBlue Bluetooth debug

#ifdef BLUETOOTH_DEBUG
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <stdarg.h>
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
BLECharacteristic *pLogCharacteristic = nullptr;

void debugLog(const char* label, uint8_t count, ...) {
    if (!pLogCharacteristic) return;
    char buffer[128];
    size_t offset = 0;
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%lu ms] %s: ", millis(), label);
    va_list args;
    va_start(args, count);
    for (uint8_t i = 0; i < count && offset < sizeof(buffer) - 4; i++) {
        uint8_t value = va_arg(args, int);
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", value);
    }
    va_end(args);
    pLogCharacteristic->setValue(buffer);
    pLogCharacteristic->notify();
}

void debugLogArray(const char* label, const uint8_t* data) {
    if (!pLogCharacteristic) return;
    char buffer[128];
    size_t offset = snprintf(buffer, sizeof(buffer), "[%lu ms] %s:", millis(), label);
    for (size_t i = 0; i < sizeof(data) && offset < sizeof(buffer) - 4; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, " %02X", data[i]);
    }
    pLogCharacteristic->setValue(buffer);
    pLogCharacteristic->notify();
}
#endif

// Main IO Frame Struct
// board/sg-cmd.h

struct sg_header {
    uint8_t frame_len;
    uint8_t addr;
    uint8_t seq_no;
    uint8_t cmd;
};

/* struct to save the version string with its length
   to fix NUL terminator issues */
struct version_info {
    const char *version;
    uint8_t length;
}; 

struct sg_nfc_poll_mifare {
    uint8_t count;
    uint8_t type;
    uint8_t id_len;
    uint8_t uid[4];
};

struct sg_nfc_poll_felica {
    uint8_t count;
    uint8_t type;
    uint8_t id_len;
    uint8_t IDm[8];
    uint8_t PMm[8];
};

static const struct version_info hw_version[] = {
    {"TN32MSEC003S H/W Ver3.0", 23},
    {"837-15286", 9},
    {"837-15396", 9}
};

static const struct version_info fw_version[] = {
    {"TN32MSEC003S F/W Ver1.2", 23},
    {"\x94", 1},
    {"\x94", 1}
};

static const struct version_info led_version[] = {
    {"15084\xFF\x10\x00\x12", 9},
    {"000-00000\xFF\x11\x40", 12},
    // maybe the same?
    {"000-00000\xFF\x11\x40", 12}
};

enum {
    // Version
    CMD_GET_FW_VERSION           = 0x30,
    CMD_GET_HW_VERSION           = 0x32,
    // NFC/RFID
    CMD_NFC_RADIO_ON             = 0x40,
    CMD_NFC_RADIO_OFF            = 0x41,
    CMD_NFC_POLL                 = 0x42,
    CMD_MIFARE_SELECT_TAG        = 0x43,
    CMD_MIFARE_UNKNOWN           = 0x44,
    // Mifare
    CMD_MIFARE_SET_KEY_AIME      = 0x50,
    CMD_MIFARE_AUTHENTICATE_AIME = 0x51,
    CMD_MIFARE_READ_BLOCK        = 0x52,
    CMD_MIFARE_SET_KEY_BANA      = 0x54,
    CMD_MIFARE_AUTHENTICATE_BANA = 0x55,
    // Device control
    CMD_TO_UPDATE_MODE           = 0x60,
    CMD_SEND_HEX_DATA            = 0x61,
    CMD_RESET                    = 0x62,
    // FeliCa
    CMD_FELICA_ENCAP             = 0x71,
    // LED
    CMD_RGB_SET_COLOR            = 0x81,
    CMD_RGB_RESET                = 0xF5,
    CMD_RGB_GET_INFO             = 0xF0,
};

// Serial IO

const uint8_t REQ_BASE_SIZE = 5;
const uint8_t RES_BASE_SIZE = 6;

static void sg_write_encode_byte(uint8_t byte);

struct __attribute__((packed)) sg_res_header {
    struct sg_header hdr;
    uint8_t status;
    uint8_t payload_len;
    uint8_t payload[256];
} res;

struct __attribute__((packed)) sg_req_header {
    struct sg_header hdr;
    uint8_t pos;
    uint8_t checksum;
    uint8_t payload_len;
    uint8_t payload[256];
} req;

#ifdef BLUETOOTH_DEBUG
void debugBLEHeader() {
    if (!pLogCharacteristic) return;
    char buffer[256] = {0};
    size_t offset = 0;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
        "REQ: hdr: {frame_len:0x%02X addr:0x%02X seq_no:0x%02X cmd:0x%02X} ",
        req.hdr.frame_len, req.hdr.addr, req.hdr.seq_no, req.hdr.cmd);

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
        "checksum:0x%02X pos:0x%02X payload_len:0x%02X ", 
        req.checksum, req.pos, req.payload_len);

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "payload:");
    for (uint8_t i = 0; i < req.payload_len; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, 
            "0x%02X ", req.payload[i]);
    }

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n");

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
        "RES: hdr: {frame_len:0x%02X addr:0x%02X seq_no:0x%02X cmd:0x%02X} ",
        res.hdr.frame_len, res.hdr.addr, res.hdr.seq_no, res.hdr.cmd);

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
        "status:0x%02X payload_len:0x%02X ", 
        res.status, res.payload_len);

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "payload:");
    for (uint8_t i = 0; i < res.payload_len; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, 
            "0x%02X ", res.payload[i]);
    }

    pLogCharacteristic->setValue(buffer);
    pLogCharacteristic->notify();
}
#endif

bool sg_read_frame() {
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        if (byte == 0xE0) {
            if (req.pos) {
                debugLog("SG Frame: Bad sync", byte);
                goto fail;
            }
            memset(&req, 0, sizeof(req));
            req.checksum = 0;
            req.pos = 1;
            continue;
        }
        if (byte == 0xD0) {
            if (!Serial.available()) {
                debugLog("SG Frame: Tralling escape", byte);
                goto fail;
            }
            byte = (uint8_t)Serial.read() + 1;
        }
        switch (req.pos) {
        case 1: req.hdr.frame_len = byte; break;
        case 2: req.hdr.addr = byte;      break;
        case 3: req.hdr.seq_no = byte;    break;
        case 4: req.hdr.cmd = byte;       break;
        case 5: req.payload_len = byte;   break;
        default:
            if (req.pos > req.hdr.frame_len + 1) {
                debugLog("SG Frame: Length overflow", byte);
                goto fail;
            }
            if (req.pos == req.hdr.frame_len + 1) {
                req.pos = 0;
                return byte == req.checksum;
            }
            req.payload[req.pos - REQ_BASE_SIZE - 1] = byte;
            break;
        }
        req.checksum += byte;
        req.pos++;
    }
fail:
    req.pos = 0;
    return false;
}

bool sg_res_init(size_t payload_len = 0, size_t status = 0) {
    res.hdr.frame_len = RES_BASE_SIZE + payload_len;
    res.hdr.addr      = req.hdr.addr;
    res.hdr.seq_no    = req.hdr.seq_no;
    res.hdr.cmd       = req.hdr.cmd;
    res.status        = status;
    res.payload_len   = payload_len;
    memset(res.payload, 0, sizeof(res.payload));
    return true;
}

bool sg_write_frame() {
    size_t pos = 0;
    uint8_t checksum = 0;
    // convert response header to bytes, instead of using union
    const uint8_t *ptr = (const uint8_t *)&res;
    
    Serial.write(0xE0);

    for (size_t i = 0; i < res.hdr.frame_len; i++) {
        uint8_t byte = ptr[i];
        checksum += byte;
        sg_write_encode_byte(byte);
    }
    sg_write_encode_byte(checksum);
    return true;
}

static void sg_write_encode_byte(uint8_t byte) {
    if (byte == 0xD0 || byte == 0xE0) {
        Serial.write(0xD0);
        Serial.write(byte - 1);
    } else {
        Serial.write(byte);
    }
}