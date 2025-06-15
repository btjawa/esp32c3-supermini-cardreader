#pragma once

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "shared.h"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "8cfc6e02-93ec-4f10-b920-d2d308beb9b7"
#define CHARACTERISTIC_UUID "e8825349-bb14-4a68-aab4-925fd6288652"

void setup_bluetooth();

void debugLog(const char* label, const uint8_t* data, size_t length);
void debugLog(const char* label, uint8_t value);
void debugLog(const char* label, uint32_t value);
void debugLog(const char* label, size_t value);

void debugLogHeader(sg_req_header &req, sg_res_header &res);