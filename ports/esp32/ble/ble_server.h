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

#ifndef _BLE_SERVER_H_
#define _BLE_SERVER_H_

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"

#include "esp_log.h"

#define BLE_SERVER_CHAR_DECLARATION_SIZE   (sizeof(uint8_t))

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

typedef struct{
	uint16_t gatts_if;
	uint16_t conn_id;
	bool has_conn;
} gatts_profile_inst_t;

typedef struct _ble_server_service_event_callbacks_t {
    void (*read_event_handler)(int attrIndex, esp_ble_gatts_cb_param_t* param, esp_gatt_rsp_t* rsp);
    void (*write_event_handler)(int attrIndex, const uint8_t *char_val_p, uint16_t char_val_len);
    void (*exec_write_event_handler)(int attrIndex, const uint8_t *char_val_p, uint16_t char_val_len);
}ble_server_service_event_callbacks_t;

typedef struct _ble_server_service_info_t {
    const char*                             name;               // name of the service
    const esp_gatts_attr_db_t*              st_def;             // pointer to the service table definition
    uint32_t                                st_num;             // the number of entries in the service table
    uint16_t*                               ht_def;             // pointer to handle table
    uint32_t                                ht_num;             // number of uint16_t elements in the handle table
    uint32_t                                ht_svc_idx;         // index of the main service within the handle table - or (usually) equivalently within the service table
    const uint8_t*                          service_uuid;       // the service uuid by which to identify this service
    uint8_t                                 service_uuid_len;   // how many bytes to compare of the service UUID. Determines which bytes are compared as well
    ble_server_service_event_callbacks_t    callbacks;          // structure of response handler functions
}ble_server_service_info_t;

typedef struct {
    uint8_t                 *prepare_buf;
    int                     prepare_len;
	uint16_t                handle;
} prepare_type_env_t;


// Public Declarations
void ble_server_begin( void );

extern const uint16_t primary_service_uuid;
extern const uint16_t character_declaration_uuid;
extern const uint16_t character_client_config_uuid;

extern const uint8_t char_prop_read_notify;
extern const uint8_t char_prop_read_write;
extern const uint8_t char_prop_read_write_notifys;

extern gatts_profile_inst_t profile;
extern uint16_t gatts_mtu;


#endif // _BLE_SERVER_H_