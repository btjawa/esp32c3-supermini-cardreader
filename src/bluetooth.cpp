#include "bluetooth.h"

static BLECharacteristic *pCharacteristic = nullptr;

class ServerCallbacks : public BLEServerCallbacks {
	void onConnect(BLEServer* pServer) override {
		pServer->getAdvertising()->start();
  }
  void onDisconnect(BLEServer* pServer) override {
		pServer->getAdvertising()->start();
  }
};

void setup_bluetooth() {
  BLEDevice::init("ESP32C3_Debug");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
	BLEDescriptor *pDescriptor = new BLE2902();
  pCharacteristic = pService->createCharacteristic(
		CHARACTERISTIC_UUID,
		BLECharacteristic::PROPERTY_READ |
		BLECharacteristic::PROPERTY_NOTIFY
	);
  pCharacteristic->addDescriptor(pDescriptor);

  pServer->setCallbacks(new ServerCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setMinInterval(0x20);
  pAdvertising->setMaxInterval(0x40);
  pAdvertising->start();
}

void debugLog(const char* label, const uint8_t* data, size_t length) {
    if (!pCharacteristic) return;
    char buffer[128];
    size_t offset = snprintf(buffer, sizeof(buffer), "[%lu ms] %s:", millis(), label);
    for (size_t i = 0; i < length && offset < sizeof(buffer) - 4; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, " %02X", data[i]);
    }
    buffer[sizeof(buffer) - 1] = '\0';
    pCharacteristic->setValue(buffer);
    pCharacteristic->notify();
}

void debugLog(const char* label, uint8_t value) {
    debugLog(label, static_cast<uint32_t>(value));
}

void debugLog(const char* label, uint32_t value) {
    if (!pCharacteristic) return;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "[%lu ms] %s: %lu", millis(), label, (unsigned long)value);
    pCharacteristic->setValue(buffer);
    pCharacteristic->notify();
}

void debugLog(const char* label, size_t value) {
    if (!pCharacteristic) return;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "[%lu ms] %s: %zu", millis(), label, value);
    pCharacteristic->setValue(buffer);
    pCharacteristic->notify();
}

void debugLogHeader(sg_req_header &req, sg_res_header &res) {
	if (!pCharacteristic) return;
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

	pCharacteristic->setValue(buffer);
	pCharacteristic->notify();
}