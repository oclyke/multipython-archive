#include "service_nus.h"

#include "py/mphal.h"   

#define TAG "SVC_NUS"

// Notification Enabled?
bool enable_nus_rx_ntf = false;

// Handle Table
uint16_t nus_handle_table[STI_NUS_NB];

// UUID definitions
const uint8_t base_uuid_nus[ESP_UUID_LEN_128] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5,   0xA9, 0xE0,   0x93, 0xF3,   0xA3, 0xB5,   0x00, 0x00, 0x40, 0x6E};
const uint8_t service_uuid_nus[ESP_UUID_LEN_128] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5,   0xA9, 0xE0,   0x93, 0xF3,   0xA3, 0xB5,   0x01, 0x00, 0x40, 0x6E};
const uint8_t char_uuid_nus_tx[ESP_UUID_LEN_128] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5,   0xA9, 0xE0,   0x93, 0xF3,   0xA3, 0xB5,   0x03, 0x00, 0x40, 0x6E};    // note: named from central's perspective, so the esp32 will actually receive data on this channel
const uint8_t char_uuid_nus_rx[ESP_UUID_LEN_128] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5,   0xA9, 0xE0,   0x93, 0xF3,   0xA3, 0xB5,   0x02, 0x00, 0x40, 0x6E};    // note: named from central's perspective, so the esp32 will actually send data out on this service

// Characteristic Value Definitions
// NUS TX characteristic
static const uint8_t  val_nus_tx[20] = {0x00};

// NUS RX Characteristic
static const uint8_t val_nus_rx[20] = {0x00};
static const uint8_t  cccd_nus[2] = {0x00, 0x00};

const esp_gatts_attr_db_t nus_gatt_db[STI_NUS_NB] =
{
    //NUS -  Service Declaration
    [STI_NUS_SVC] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
    // sizeof(spp_service_uuid), sizeof(spp_service_uuid), (uint8_t *)&spp_service_uuid}},
    sizeof(service_uuid_nus), sizeof(service_uuid_nus), (uint8_t *)service_uuid_nus}},

        //SPP -  data receive characteristic Declaration
        [STI_NUS_TX_CHAR] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
        BLE_SERVER_CHAR_DECLARATION_SIZE,BLE_SERVER_CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},

            //SPP -  data receive characteristic Value
            [STI_NUS_TX_VAL] =
            // {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_data_receive_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)char_uuid_nus_tx, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
            NUS_DATA_MAX_LEN,sizeof(val_nus_tx), (uint8_t *)val_nus_tx}},

        //SPP -  data notify characteristic Declaration
        [STI_NUS_RX_CHAR] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
        BLE_SERVER_CHAR_DECLARATION_SIZE,BLE_SERVER_CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},

            //SPP -  data notify characteristic Value
            [STI_NUS_RX_VAL] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)&char_uuid_nus_rx, ESP_GATT_PERM_READ,
            NUS_DATA_MAX_LEN, sizeof(val_nus_rx), (uint8_t *)val_nus_rx}},

            //SPP -  data notify characteristic - Client Characteristic Configuration Descriptor
            [STI_NUS_RX_CFG] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
            sizeof(uint16_t),sizeof(cccd_nus), (uint8_t *)cccd_nus}},
};



// GATTS Event Handler Definitions
void nus_read_event_handler(int attrIndex, esp_ble_gatts_cb_param_t* param, esp_gatt_rsp_t* rsp){
    ESP_LOGD(TAG, "read event handler, attrIndex (%d)", attrIndex);
}

void nus_write_event_handler(int attrIndex, const uint8_t *char_val_p, uint16_t char_val_len){
    ESP_LOGD(TAG, "write event handler, attrIndex (%d)", attrIndex);

    switch(attrIndex){
        case STI_NUS_TX_VAL :
            ESP_LOGD(TAG, "TX_VAL");
            for( size_t indi = 0; indi < char_val_len; indi++ ){    // when a write event occurs for the TX value place that into the stdin_ringbuf buffer for micropython
                uint8_t c = *((uint8_t*)(char_val_p) + indi);
                // printf("putting in char: %d\n", c);
                ringbuf_put(&stdin_ringbuf, c);
            }
            break;

        case STI_NUS_RX_VAL :
            ESP_LOGD(TAG, "RX_VAL");    // nothing to do if receiving into the RX_VAL
            break;

        case STI_NUS_RX_CFG :
            ESP_LOGI(TAG, "RX_CFG");    // This is the CCC for RX, to enable notifications
            if((char_val_len == 2)&&(char_val_p[0] == 0x01)&&(char_val_p[1] == 0x00)){
                ESP_LOGD(TAG, "Enable NUS RX Notifications");
                enable_nus_rx_ntf = true;
            }else if((char_val_len == 2)&&(char_val_p[0] == 0x00)&&(char_val_p[1] == 0x00)){
                ESP_LOGD(TAG, "Disable NUS RX Notifications");
                enable_nus_rx_ntf = false;
            }
            break;
    }
}

void nus_exec_write_event_handler(int attrIndex, const uint8_t *char_val_p, uint16_t char_val_len){
    ESP_LOGD(TAG, "exec write event handler, attrIndex (%d)", attrIndex);
    nus_write_event_handler( attrIndex, char_val_p, char_val_len);  // ble_server.c handles long writes so that this callback can simply call the regular write event callback
}


// Simplified RX send function
void nus_rx_notify(const char *str, uint32_t len){
    if(( profile.gatts_if != ESP_GATT_IF_NONE ) && ( enable_nus_rx_ntf == true )){
        // send len bytes in (gatts_mtu - 3) sized packets
        // todo: you could consider adding some kind of header/footer to indicate the packet number of multi-packet transfers... (see esp-idf example)

        uint8_t* substr = (uint8_t*)str;
        uint32_t tx_len = (gatts_mtu - 3);

        while( len > (gatts_mtu - 3) ){
            // Send additional packets
            esp_ble_gatts_send_indicate(profile.gatts_if, profile.conn_id, nus_handle_table[STI_NUS_RX_VAL], tx_len, substr, false);
            substr += tx_len;   // increment the send pointer
            len -= tx_len;
            // ESP_LOGE(TAG, "sending pre packet");
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        if( len > 0 ){
            // Send the final packet
            tx_len = len;
            esp_ble_gatts_send_indicate(profile.gatts_if, profile.conn_id, nus_handle_table[STI_NUS_RX_VAL], len, substr, false);
            // ESP_LOGE(TAG, "sending last packet");
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
    }else{
        ESP_LOGD(TAG, "RX Notify Not Enabled, or no GATTS_IF");
    }
}