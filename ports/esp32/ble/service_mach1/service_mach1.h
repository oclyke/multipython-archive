/*
Copyright 2019 Owen Lyke

Permission is hereby granted, free of charge, to any person 
obtaining a copy of this software and associated documentation 
files (the "Software"), to deal in the Software without restriction, 
including without limitation the rights to use, copy, modify, merge, 
publish, distribute, sublicense, and/or sell copies of the Software, 
and to permit persons to whom the Software is furnished to do so, 
subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef _BLE_SERVICE_MACH1_H_
#define _BLE_SERVICE_MACH1_H_

#include "ble_server.h"

// GATTS Table Enumeration
enum {
    STI_MACH1_SVC,

    STI_MACH1_CHAR_PH,  // placeholder characteristic with read+notify
    STI_MACH1_VAL_PH,
    STI_MACH1_CFG_PH,

    STI_MACH1_NB        // number of elements in service table
};

// // UUID declarations
// extern const uint8_t base_uuid_mach1[ESP_UUID_LEN_128];
extern const uint8_t service_uuid_mach1[ESP_UUID_LEN_128];

// Handle TableDeclarations
extern uint16_t mach1_handle_table[STI_MACH1_NB];

// Service Table Declaration
extern const esp_gatts_attr_db_t mach1_gatt_db[STI_MACH1_NB];

// // Callbacks Declarations
// #define MACH1_NUM_GATTS_CALLBACKS 0
// extern ble_server_service_event_callback_t mach1_gatts_callbacks[NUS_NUM_GATTS_CALLBACKS];


#endif // _BLE_SERVICE_MACH1_H_