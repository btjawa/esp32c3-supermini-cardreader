#include "sega.h"

static uint8_t aime_key[6];
static uint8_t bana_key[6];

static sg_res_header res;
static sg_req_header req;

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

static bool sg_read_frame() {
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

static bool sg_res_init(size_t payload_len = 0, size_t status = 0) {
	res.hdr.frame_len = RES_BASE_SIZE + payload_len;
	res.hdr.addr      = req.hdr.addr;
	res.hdr.seq_no    = req.hdr.seq_no;
	res.hdr.cmd       = req.hdr.cmd;
	res.status        = status;
	res.payload_len   = payload_len;
	memset(res.payload, 0, sizeof(res.payload));
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

static bool sg_write_frame() {
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

void sega::handle_frame() {
	if (!sg_read_frame()) return;
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
	debugLogHeader(req, res);
	sg_write_frame();
}