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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "driver/uart.h"
#include "string.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"

#include "ble_server.h"

// Include Service Headers (also make sure that the c source is added to the compilation)
#include "service_mach1.h"
#include "service_nus.h"


// Define TAG
#define TAG "BLE_SERVER"


// App Profile Definitions
#define SERVER_NUM_PROFILE      1
#define SERVER_PROFILE_INDEX    0
#define SERVER_PROFILE_ID       0x56
#define SERVER_SVC_INST_ID      0


//**********************************
// 
// Forward Declarations
//
//**********************************

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gatts_proc_read(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_read_env, esp_ble_gatts_cb_param_t *param, uint8_t *p_rsp_v, uint16_t v_len);
void gatts_proc_long_read(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_read_env, esp_ble_gatts_cb_param_t *param);
uint16_t getAttributeIndexByServiceHandle( uint16_t attributeHandle, ble_server_service_info_t* info );
void example_prepare_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);

//**********************************
// 
// Globals
//
//**********************************

// Services to Support:
#define BLE_SERVER_NUM_SERVICES 2
ble_server_service_info_t ble_server_services[BLE_SERVER_NUM_SERVICES] = {
    /*
    Add your service definitions here: e.g.
    {   // service name
        .service_uuid       = pointer to the service uuid (16, 32, or 128 bits) in a contiguous array of uint8_t types
        .service_uuid_len   = length of provided uuid (2, 4, or 16)
        .callbacks_info     = pointer_to_array_of_gatts_event_callbacks
        .callbacks_num      = number of callbacks to check for this service (not to exceed the number in the array)
    }
    */

    {   // Mach1 Service
        .name               = "Mach1",
        .st_def             = mach1_gatt_db,
        .st_num             = STI_MACH1_NB,
        .ht_def             = mach1_handle_table,
        .ht_num             = STI_MACH1_NB,
        .ht_svc_idx         = STI_MACH1_SVC,
        .service_uuid       = service_uuid_mach1,    
        .service_uuid_len   = ESP_UUID_LEN_128,   
        .callbacks          = {
            .read_event_handler             = NULL,
            .write_event_handler            = NULL,
            .exec_write_event_handler       = NULL,
        },                               
    },  
    {    // Nordic Uart Service
        .name               = "NUS",
        .st_def             = nus_gatt_db,
        .st_num             = STI_NUS_NB,
        .ht_def             = nus_handle_table,
        .ht_num             = STI_NUS_NB,
        .ht_svc_idx         = STI_NUS_SVC,
        .service_uuid       = service_uuid_nus,      
        .service_uuid_len   = ESP_UUID_LEN_128,   
        .callbacks          = {
            .read_event_handler             = nus_read_event_handler,
            .write_event_handler            = nus_write_event_handler,
            .exec_write_event_handler       = nus_exec_write_event_handler,
        },
    },          
};

static prepare_type_env_t prepare_write_env;
static prepare_type_env_t prepare_read_env;

#define PREPARE_BUF_MAX_SIZE        1024
uint16_t gatts_mtu = 23;

gatts_profile_inst_t profile = {
    .gatts_if = ESP_GATT_IF_NONE,
    .conn_id = 0,
    .has_conn = 0,
};

// Standard UUIDs
const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

const uint8_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ|ESP_GATT_CHAR_PROP_BIT_NOTIFY;
const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE_NR|ESP_GATT_CHAR_PROP_BIT_READ;
const uint8_t char_prop_read_write_notify = ESP_GATT_CHAR_PROP_BIT_READ|ESP_GATT_CHAR_PROP_BIT_WRITE_NR|ESP_GATT_CHAR_PROP_BIT_NOTIFY;


// Advertising parameters
static esp_ble_adv_params_t ble_server_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Advertising data
static uint8_t server_adv_data[28] = {
    0x02,0x01,0x06,                                                                             // flags
    0x11,0x06,0x69,0xa4,0x22,0xc9,0xc7,0x63,0x45,0xbe,0x42,0x40,0x84,0x9c,0x01,0x00,0x0b,0x1a,  // incomplete list of 128-bit UUIDs - use Mach1 UUID for uniqueness
    0x06,0x08,0x4D,0x61,0x63,0x68,0x31,                                                         // Shortened Local Name
};

// App
static struct gatts_profile_inst server_profile_tab[SERVER_NUM_PROFILE] = {
    [SERVER_PROFILE_INDEX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};





static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    // esp_err_t err;
    ESP_LOGD(TAG, "GAP_EVT, event %d", event);

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&ble_server_adv_params);
        break;

	case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
		/* advertising start complete event to indicate advertising start successfully or failed */
		if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS){ ESP_LOGE(TAG,"advertising start failed"); }
		else{ ESP_LOGD(TAG,"advertising start successfully"); }
		break;

	case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
		if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){ ESP_LOGE(TAG,"Advertising stop failed"); }
		else{ ESP_LOGD(TAG,"Stop adv successfully"); }
		break;

	case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
		ESP_LOGD(TAG,"update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
			  param->update_conn_params.status,
			  param->update_conn_params.min_int,
			  param->update_conn_params.max_int,
			  param->update_conn_params.conn_int,
			  param->update_conn_params.latency,
			  param->update_conn_params.timeout);
		break;

	default:
		break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            server_profile_tab[SERVER_PROFILE_INDEX].gatts_if = gatts_if;
        } else {
            ESP_LOGE(TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }
    do {
        int idx;
        for (idx = 0; idx < SERVER_NUM_PROFILE; idx++) {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == server_profile_tab[idx].gatts_if) {
                if (server_profile_tab[idx].gatts_cb) {
                    server_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param){
    ESP_LOGD(TAG, "GATTS profile event = %x",event);
    switch (event) {
    	case ESP_GATTS_REG_EVT :

        	ESP_LOGI(TAG, "config adv data: %s %d", __func__, __LINE__);
        	esp_ble_gap_config_adv_data_raw((uint8_t *)server_adv_data, sizeof(server_adv_data));

            // Automatically register attribute tables for declared services here
            esp_err_t create_attr_ret = ESP_OK;
            for( uint32_t indi = 0; indi < BLE_SERVER_NUM_SERVICES; indi++ ){
                ble_server_service_info_t* info = &(ble_server_services[indi]);
                create_attr_ret = esp_ble_gatts_create_attr_tab(info->st_def, gatts_if, info->st_num, SERVER_SVC_INST_ID);
                if (create_attr_ret){
                    ESP_LOGE(TAG, "create attr table failed, service \"%s\", error code = %x", info->name, create_attr_ret);
                }else{
                    ESP_LOGI(TAG, "create attr table success, service \"%s\", entries = %d", info->name, info->st_num);
                }
            }
            break;


    	case ESP_GATTS_CREAT_ATTR_TAB_EVT :

            ESP_LOGD(TAG, "create attr table event");
            
            // Automatically start external services for registered services
            for( uint32_t indi = 0; indi < BLE_SERVER_NUM_SERVICES; indi++ ){
                ble_server_service_info_t* info = &(ble_server_services[indi]);
                if( memcmp( (void*)info->service_uuid, (void*)param->add_attr_tab.svc_uuid.uuid.uuid128, info->service_uuid_len ) == 0 ){
                    ESP_LOGD(TAG, "matched uuid for service \"%s\"", info->name);

                    if(param->add_attr_tab.num_handle != info->st_num){ ESP_LOGE(TAG,"create attribute table abnomaly, num_handle (%d) isn't equal to INFO_NB(%d)", param->add_attr_tab.num_handle, info->st_num); }
				    else{
                        ESP_LOGD(TAG,"create attribute table successfully, the number handle = %d",param->add_attr_tab.num_handle);
                        memcpy(info->ht_def, param->add_attr_tab.handles, info->ht_num*sizeof(uint16_t) );
                        esp_ble_gatts_start_service( info->ht_def[info->ht_svc_idx] );
                    }
                }
            }
            break;

    	case ESP_GATTS_READ_EVT :
            ESP_LOGD(TAG, "GATT_READ_EVT, conn_id %d, trans_id %d, handle %d offset %d", param->read.conn_id, param->read.trans_id, param->read.handle, param->read.offset);

            // Note that if char is defined with ESP_GATT_AUTO_RSP, then this event is triggered after the internal value has been sent.  So this event is not very useful.
            // Otherwise if it is ESP_GATT_RSP_BY_APP, then call helper function gatts_proc_read()
            if (!param->read.is_long) {
                // If is.long is false then this is the first (or only) request to read data
                int               attrIndex;
                esp_gatt_rsp_t rsp;
                rsp.attr_value.len = 0;

                // Automatically check read events for registered services
                for( uint32_t indi = 0; indi < BLE_SERVER_NUM_SERVICES; indi++ ){
                    ble_server_service_info_t* info = &(ble_server_services[indi]);

                    attrIndex = getAttributeIndexByServiceHandle( param->read.handle, info );
                    if( attrIndex < info->ht_num ){
                        ESP_LOGD(TAG, "Matched attrIndex for service: \"%s\" -- calling read event callback (0x%08X)", info->name, (uint32_t)info->callbacks.read_event_handler );
                        if( info->callbacks.read_event_handler != NULL ){
                            info->callbacks.read_event_handler(attrIndex, param, &rsp);
                        }
                    }
                }

                // Helper function sends what it can (up to MTU size) and buffers the rest for later.
                gatts_proc_read(gatts_if, &prepare_read_env, param, rsp.attr_value.value, rsp.attr_value.len);
                }
            else  // a continuation of a long read.
            {
                // Dont invoke the handle#SERVICE#ReadEvent again, just keep pumping out buffered data.
                gatts_proc_long_read(gatts_if, &prepare_read_env, param);
            }
            break;


    	case ESP_GATTS_WRITE_EVT :
        {
            esp_gatt_status_t status = ESP_GATT_WRITE_NOT_PERMIT;
            int attrIndex;

            if (!param->write.is_prep){
                // This section handles writes where the length is less than or = MTU.  (not long writes)
                // (But the length of write data must always be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.)
                ESP_LOGD(TAG, "GATT_WRITE_EVT, (short) handle = %d, value len = %d, value :", param->write.handle, param->write.len);
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, param->write.value, param->write.len, ESP_LOG_DEBUG);

                // Automatically check read events for registered services
                for( uint32_t indi = 0; indi < BLE_SERVER_NUM_SERVICES; indi++ ){
                    ble_server_service_info_t* info = &(ble_server_services[indi]);

                    attrIndex = getAttributeIndexByServiceHandle( param->read.handle, info );
                    if( attrIndex < info->ht_num ){
                        ESP_LOGD(TAG, "Matched attrIndex for service: \"%s\" -- calling write event callback (0x%08X)", info->name, (uint32_t)info->callbacks.write_event_handler );
                        if( info->callbacks.write_event_handler != NULL ){
                            info->callbacks.write_event_handler(attrIndex, param->write.value, param->write.len);
                        }
                    }
                }

                /* send response when param->write.need_rsp is true*/
                if (param->write.need_rsp){
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
                }
            }else{
                /* handle prepare write */
                ESP_LOGI(TAG, "GATT_WRITE_EVT, (prepare) handle = %d, value len = %d, value :", param->write.handle, param->write.len);
                prepare_write_env.handle = param->write.handle;  // keep track of the handle for ESP_GATTS_EXEC_WRITE_EVT since it doesn't provide it.
                // Note: if characteristic set to ESP_GATT_AUTO_RSP then long write is handled internally.
                // Otherwise if ESP_GATT_RSP_BY_APP, then call helper function here (and later handle the final write in ESP_GATTS_EXEC_WRITE_EVT)
                example_prepare_write_event_env(gatts_if, &prepare_write_env, param);
            }
        }
            break;

    	case ESP_GATTS_EXEC_WRITE_EVT :
        // attrIndex, char_val_p, char_val_len
        {
            esp_gatt_status_t status = ESP_GATT_OK;
            int attrIndex;
            // This section handles long writes where the length is greater than MTU.
            // (But the length of write data must always be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.)
            ESP_LOGI(TAG, "ESP_GATTS_EXEC_WRITE_EVT");

            if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) // Make sure long write was not aborted.
            {
                uint16_t char_val_len = 0;
                const uint8_t *char_val_p;
                if (prepare_write_env.prepare_buf == NULL)
                {
                    // Note: if characteristic set to ESP_GATT_AUTO_RSP, long write is handled internally.  Get value from API.
                    esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->write.handle, &char_val_len, &char_val_p);
                    if (get_attr_ret != ESP_OK){
                        ESP_LOGE(TAG,"ERROR: %d", get_attr_ret);
                    }
                }
                else  // Otherwise if ESP_GATT_RSP_BY_APP
                {
                    char_val_p = prepare_write_env.prepare_buf;
                    char_val_len = prepare_write_env.prepare_len;
                }

                // Automatically check write events for registered services
                for( uint32_t indi = 0; indi < BLE_SERVER_NUM_SERVICES; indi++ ){
                    ble_server_service_info_t* info = &(ble_server_services[indi]);

                    attrIndex = getAttributeIndexByServiceHandle( param->read.handle, info );
                    if( attrIndex < info->ht_num ){
                        ESP_LOGD(TAG, "Matched attrIndex for service: \"%s\" -- calling exec write event callback (0x%08X)", info->name, (uint32_t)info->callbacks.exec_write_event_handler );
                        if( info->callbacks.exec_write_event_handler != NULL ){
                            info->callbacks.exec_write_event_handler(attrIndex, param->write.value, param->write.len);
                        }
                    }
                }

            }
            else // not ESP_GATT_PREP_WRITE_EXEC
            {
                ESP_LOGW(TAG,"ESP_GATT_PREP_WRITE_CANCEL");
            }

            example_exec_write_event_env(&prepare_write_env, param); // this cleans-up the long-write.
            /* Always send response for EXEC_WRITE */
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
        }
            break;

    	case ESP_GATTS_MTU_EVT :
            ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            gatts_mtu = param->mtu.mtu;
            break;

    	case ESP_GATTS_CONF_EVT :
            // ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT, status = %d", param->conf.status);
            break;

    	case ESP_GATTS_UNREG_EVT :  
            ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
            break;

    	case ESP_GATTS_CONNECT_EVT :
        	gatts_mtu = 23;
            profile.conn_id = param->connect.conn_id;
            profile.has_conn = true;
            profile.gatts_if = gatts_if;
            ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
		    ESP_LOG_BUFFER_HEX_LEVEL(TAG, param->connect.remote_bda, 6, ESP_LOG_INFO);
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
            conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
            //start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
            break;

    	case ESP_GATTS_DISCONNECT_EVT :
		    profile.has_conn = false;
            ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, reason = %d", param->disconnect.reason);
            esp_ble_gap_start_advertising(&ble_server_adv_params);
            break;

        case ESP_GATTS_CREATE_EVT :
        case ESP_GATTS_ADD_INCL_SRVC_EVT :
        case ESP_GATTS_ADD_CHAR_EVT :
        case ESP_GATTS_ADD_CHAR_DESCR_EVT :
    	case ESP_GATTS_DELETE_EVT :
    	case ESP_GATTS_START_EVT :
    	case ESP_GATTS_STOP_EVT :
    	case ESP_GATTS_OPEN_EVT :
    	case ESP_GATTS_CANCEL_OPEN_EVT :
    	case ESP_GATTS_CLOSE_EVT :
    	case ESP_GATTS_LISTEN_EVT :
    	case ESP_GATTS_CONGEST_EVT :
        case ESP_GATTS_RESPONSE_EVT :
        case ESP_GATTS_SET_ATTR_VAL_EVT :
        case ESP_GATTS_SEND_SERVICE_CHANGE_EVT :
        default:
            break;
    }

    // todo: study the multi-service implementation of calling functions for services and try to work out a general way to do it...

    // // Now go through registered services and call any applicable callbacks
    // for( uint32_t indi = 0; indi < BLE_SERVER_NUM_SERVICES; indi++ ){
    //     ble_server_service_info_t* info = &(ble_server_services[indi]);
    //     ESP_LOGI(TAG, "checking callbacks for registered service number %d", indi);

    //     printf("incoming service uuid:");
    //     for(uint32_t bite = 0; bite < ESP_UUID_LEN_128; bite++){
    //         printf("0x%02x, ", param->add_attr_tab.svc_uuid.uuid.uuid128[bite]);
    //     }
    //     printf("\n");

    //     if( memcmp( (void*)info->service_uuid, (void*)param->add_attr_tab.svc_uuid.uuid.uuid128, info->service_uuid_len ) == 0 ){
    //         ESP_LOGI(TAG, "callback: matched uuid for service \"%s\", callbacks_info = 0x%08X, calbacks_num = %d", info->name, (uint32_t)info->callbacks_info, info->callbacks_num);
    //         if( info->callbacks_info != NULL ){
    //             for( uint32_t indj = 0; indj < info->callbacks_num; indj++ ){
    //                 ble_server_service_event_callback_t* cb_info = &(info->callbacks_info[indj]);
    //                 if( cb_info->event == event ){
    //                     ESP_LOGI(TAG, "callback: matched on event (%d)", event);
    //                     if( cb_info->event_handler != NULL ){
    //                         ESP_LOGI(TAG, "callback: calling");
    //                         cb_info->event_handler( event, gatts_if, param);
    //                     }
    //                 }
    //             }
    //         }
    //     }
    // }
}





// main routine to begin the BLE server
void ble_server_begin( void ){
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)); // todo: does this interfere with normal bluetooth?

    ret = esp_bt_controller_init(&bt_cfg);              if (ret) { ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret)); return; }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);    if (ret) { ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret)); return; }

    ESP_LOGI(TAG, "%s init bluetooth", __func__);
   
    ret = esp_bluedroid_init();                         if (ret) { ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret)); return; }
    ret = esp_bluedroid_enable();                       if (ret) { ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret)); return; }

    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(SERVER_PROFILE_ID);

    // static uint32_t temp_count = 0;
    // char msg[128];

    // while(1){
    //     // todo: remove this if you can't figure out why micropython is not starting
    //     vTaskDelay(1000/portTICK_PERIOD_MS);

    //     snprintf(msg, 128, "testing an extra long write with a count: %d", temp_count++);
    //     nus_rx_notify(msg, strlen(msg));
    // }
}

// support read and long read
void gatts_proc_read(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_read_env, esp_ble_gatts_cb_param_t *param, uint8_t *p_rsp_v, uint16_t v_len)
{
	if(!param->read.need_rsp) {
		return;
	}
	uint16_t value_len = gatts_mtu - 1;
	if(v_len - param->read.offset < (gatts_mtu - 1)) { // read response will fit in one MTU?
		value_len = v_len - param->read.offset;
	}
	else if (param->read.offset == 0) // it's the start of a long read  (could also use param->read.is_long here?)
	{
		ESP_LOGI(TAG, "long read, handle = %d, value len = %d", param->read.handle, v_len);

		if (v_len > PREPARE_BUF_MAX_SIZE) {
			ESP_LOGE(TAG, "long read too long");
			return;
		}
		if (prepare_read_env->prepare_buf != NULL) {
			ESP_LOGW(TAG, "long read buffer not free");
			free(prepare_read_env->prepare_buf);
			prepare_read_env->prepare_buf = NULL;
		}

		prepare_read_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
		prepare_read_env->prepare_len = 0;
		if (prepare_read_env->prepare_buf == NULL) {
			ESP_LOGE(TAG, "long read no mem");
			return;
		}
		memcpy(prepare_read_env->prepare_buf, p_rsp_v, v_len);
		prepare_read_env->prepare_len = v_len;
		prepare_read_env->handle = param->read.handle;
	}
	esp_gatt_rsp_t rsp;
	memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
	rsp.attr_value.handle = param->read.handle;
	rsp.attr_value.len = value_len;
	rsp.attr_value.offset = param->read.offset;
	memcpy(rsp.attr_value.value, &p_rsp_v[param->read.offset], value_len);
	esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);

}

// continuation of read, use buffered value
void gatts_proc_long_read(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_read_env, esp_ble_gatts_cb_param_t *param)
{

	if (prepare_read_env->prepare_buf && (prepare_read_env->handle == param->read.handle)) // something buffered?
	{
		gatts_proc_read(gatts_if, prepare_read_env, param, prepare_read_env->prepare_buf, prepare_read_env->prepare_len);
		if(prepare_read_env->prepare_len - param->read.offset < (gatts_mtu - 1)) // last read?
		{
			free(prepare_read_env->prepare_buf);
			prepare_read_env->prepare_buf = NULL;
			prepare_read_env->prepare_len = 0;
			ESP_LOGI(TAG,"long_read ended");
		}
	}
	else
	{
		ESP_LOGE(TAG,"long_read not buffered");
	}
}

uint16_t getAttributeIndexByServiceHandle( uint16_t attributeHandle, ble_server_service_info_t* info ){
	// Get the attribute index in the attribute table by the returned handle

    uint16_t attrIndex = info->ht_num;
    uint16_t index;

    for( index = 0; index < info->ht_num; index++ ){
        if( info->ht_def[index] == attributeHandle ){
            attrIndex = index;
            break;
        }
    }
    return attrIndex;
}

void example_prepare_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGI(TAG, "prepare write, handle = %d, value len = %d", param->write.handle, param->write.len);
    esp_gatt_status_t status = ESP_GATT_OK;
    if (prepare_write_env->prepare_buf == NULL) {
        prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
        prepare_write_env->prepare_len = 0;
        if (prepare_write_env->prepare_buf == NULL) {
            ESP_LOGE(TAG, "%s, Gatt_server prep no mem", __func__);
            status = ESP_GATT_NO_RESOURCES;
        }
    } else {
        if(param->write.offset > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_OFFSET;
        } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_ATTR_LEN;
        }
    }
    /*send response when param->write.need_rsp is true */
    if (param->write.need_rsp){
        esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
        if (gatt_rsp != NULL){
            gatt_rsp->attr_value.len = param->write.len;
            gatt_rsp->attr_value.handle = param->write.handle;
            gatt_rsp->attr_value.offset = param->write.offset;
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
            esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            if (response_err != ESP_OK){
               ESP_LOGE(TAG, "Send response error");
            }
            free(gatt_rsp);
        }else{
            ESP_LOGE(TAG, "%s, malloc failed", __func__);
        }
    }
    if (status != ESP_GATT_OK){
        return;
    }
    memcpy(prepare_write_env->prepare_buf + param->write.offset,
           param->write.value,
           param->write.len);
    prepare_write_env->prepare_len += param->write.len;
}

void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param){
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}



