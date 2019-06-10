#include "service_mach1.h"

// Handle Table
uint16_t mach1_handle_table[STI_MACH1_NB];

// UUID definitions
// MACH1 Service (To be recognized on advertising, and to provide direct access to certain relevant items in the future)
const uint8_t base_uuid_mach1[ESP_UUID_LEN_128]     = { 0x69, 0xa4, 0x22, 0xc9, 0xc7, 0x63,   0x45, 0xbe,   0x42, 0x40,   0x84, 0x9c,   0x00, 0x00, 0x0b, 0x1a };
const uint8_t service_uuid_mach1[ESP_UUID_LEN_128]  = { 0x69, 0xa4, 0x22, 0xc9, 0xc7, 0x63,   0x45, 0xbe,   0x42, 0x40,   0x84, 0x9c,   0x01, 0x00, 0x0b, 0x1a };
const uint8_t char_uuid_mach1_ph[ESP_UUID_LEN_128]  = { 0x69, 0xa4, 0x22, 0xc9, 0xc7, 0x63,   0x45, 0xbe,   0x42, 0x40,   0x84, 0x9c,   0x02, 0x00, 0x0b, 0x1a };

// Characteristic Values
// Mach1 Service - placeholder characteristic
static const uint8_t val_mach1_ph[1] = {0x00};
static const uint8_t  cccd_mach1_ph[2] = {0x00, 0x00};

// Service Structure
const esp_gatts_attr_db_t mach1_gatt_db[STI_MACH1_NB] = {
    //MACH1 -  Service Declaration
    [STI_MACH1_SVC] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
    // sizeof(spp_service_uuid), sizeof(spp_service_uuid), (uint8_t *)&spp_service_uuid}},
    sizeof(service_uuid_mach1), sizeof(service_uuid_mach1), (uint8_t *)service_uuid_mach1}},

        //MACH1 - placeholder characteristic declaration
        [STI_MACH1_CHAR_PH] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
        BLE_SERVER_CHAR_DECLARATION_SIZE,BLE_SERVER_CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},

            //MACH1 -  data notify characteristic Value
            [STI_MACH1_VAL_PH] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)&char_uuid_mach1_ph, ESP_GATT_PERM_READ,
            sizeof(val_mach1_ph), sizeof(val_mach1_ph), (uint8_t *)val_mach1_ph}},

            //MACH1 -  data notify characteristic - Client Characteristic Configuration Descriptor
            [STI_MACH1_CFG_PH] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
            sizeof(uint16_t),sizeof(cccd_mach1_ph), (uint8_t *)cccd_mach1_ph}},
};