#include "spice.h"
// #include "connection.h"
// #include "wrappers.h"

// https://github.com/spice2x/spice2x.github.io/tree/main/src/spice2x/api/resources/arduino

// #define SPICEAPI_INTERFACE Serial
// spiceapi::Connection CON(512);

void spice::insert_card() {
    uint8_t luid[10];
    uint8_t id_len;
    uint8_t IDm[8];
    uint8_t PMm[8];
    uint16_t sys_code[4] = { 0 };
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, luid, &id_len, NFC_TIMEOUT)) {
        /* UNIMPLEMENTED YET */
    } else if (nfc.felica_Polling(0xFFFF, 0x01, IDm, PMm, sys_code, NFC_TIMEOUT)) {
        char id_str[20];
        sprintf(id_str, "%02X%02X%02X%02X%02X%02X%02X%02X",
                IDm[0], IDm[1], IDm[2], IDm[3], IDm[4], IDm[5], IDm[6], IDm[7]);
        // spiceapi::card_insert(CON, dipsw2 ? 1 : 0, id_str);
    }
    return;
}