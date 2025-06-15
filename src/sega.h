#pragma once

#include "bluetooth.h"
#include "shared.h"

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

#define REQ_BASE_SIZE 5
#define RES_BASE_SIZE 6

namespace sega {
	void handle_frame();
}