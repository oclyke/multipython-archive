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

#ifndef _BLE_SERVICE_NUS_H_
#define _BLE_SERVICE_NUS_H_

#include "ble_server.h"

#define NUS_DATA_MAX_LEN (512)

enum {
    STI_NUS_SVC,

    STI_NUS_TX_CHAR,
    STI_NUS_TX_VAL,

    STI_NUS_RX_CHAR,
    STI_NUS_RX_VAL,
    STI_NUS_RX_CFG,

    STI_NUS_NB,
};

// RX Notify Enabled
extern bool enable_nus_rx_ntf;

// // UUID declarations
// extern const uint8_t base_uuid_nus[ESP_UUID_LEN_128];
extern const uint8_t service_uuid_nus[ESP_UUID_LEN_128];

// Handle TableDeclarations
extern uint16_t nus_handle_table[STI_NUS_NB];

// Service Table Declaration
extern const esp_gatts_attr_db_t nus_gatt_db[STI_NUS_NB];

// GATTS Event Handler Declarations
void nus_read_event_handler(int attrIndex, esp_ble_gatts_cb_param_t* param, esp_gatt_rsp_t* rsp);
void nus_write_event_handler(int attrIndex, const uint8_t *char_val_p, uint16_t char_val_len);
void nus_exec_write_event_handler(int attrIndex, const uint8_t *char_val_p, uint16_t char_val_len);

// Simplified RX send function
void nus_rx_notify(const char *str, uint32_t len);

#endif // _BLE_SERVICE_NUS_H_