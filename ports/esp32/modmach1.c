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
#include "modmach1.h"

#include <string.h>

// #include "py/binary.h"
// #include "py/builtin.h"
// #include "py/compile.h"
#include "py/mperrno.h"
#include "py/gc.h"
// #include "py/mphal.h"
#include "py/mpstate.h"
#include "py/nlr.h"
#include "py/obj.h"
// #include "py/parse.h"
#include "py/runtime.h"
// #include "py/stackctrl.h"

#include "esp_log.h"
#include "esp_spiram.h"
#include "driver/gpio.h"


#include "ota/wifi_sta.h"   // WIFI module configuration, connecting to an access point.
#include "ota/iap_https.h"  // Coordinating firmware updates

#define TAG "Mach1"

char    mach1_device_name[MACH1_DEVICE_NAME_MAX_LEN];   // Will fill this at run-time with snprintf("Mach1 LED %02X.%02X.%02X.%02X.%02X.%02X", chip_id[0], chip_id[1], chip_id[2], chip_id[3], chip_id[4], chip_id[5] );
uint8_t mach1_chip_id[6];                                     // Set this at runtime with esp_efuse_mac_get_default(chip_id)

#define OTA_SERVER_HOST_NAME      "reverb.echoictech.com"
#define OTA_SERVER_METADATA_PATH  "/ota/ota.txt"

// Provide the Root CA certificate for chain validation.
// (gotten for reverb.echoichtech.com with command line:
//  openssl s_client -showcerts -servername reverb.example.com -connect reverb.echoictech.com:443 </dev/null)
#define OTA_SERVER_ROOT_CA_PEM \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/\n" \
    "MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n" \
    "DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow\n" \
    "SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT\n" \
    "GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC\n" \
    "AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF\n" \
    "q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8\n" \
    "SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0\n" \
    "Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA\n" \
    "a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj\n" \
    "/PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T\n" \
    "AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG\n" \
    "CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv\n" \
    "bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k\n" \
    "c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw\n" \
    "VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC\n" \
    "ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz\n" \
    "MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu\n" \
    "Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF\n" \
    "AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo\n" \
    "uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/\n" \
    "wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu\n" \
    "X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG\n" \
    "PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6\n" \
    "KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==\n" \
    "-----END CERTIFICATE-----\n"

// Provide the Peer certificate for certificate pinning.
// (copied from reverb.echoictech.com using 
// openssl s_client -showcerts -servername reverb.example.com -connect reverb.echoictech.com:443 </dev/null)
#define OTA_PEER_PEM \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIFfzCCBGegAwIBAgISAw6vOHbEKpsy7G8QguNI6SiSMA0GCSqGSIb3DQEBCwUA\n" \
    "MEoxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MSMwIQYDVQQD\n" \
    "ExpMZXQncyBFbmNyeXB0IEF1dGhvcml0eSBYMzAeFw0xOTA0MjExNzM1MTVaFw0x\n" \
    "OTA3MjAxNzM1MTVaMBkxFzAVBgNVBAMTDmVjaG9pY3RlY2guY29tMIIBIjANBgkq\n" \
    "hkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0gKXSjl/KD6c70SRnajgBjj2pZuw6Ko3\n" \
    "f7Xq5Ghd/FVTB12CkYHpZZy9ARWFle5SC/NjMQ7USPwNQ4sCaHilUYoZZ7+u2OIC\n" \
    "T5lA/0ctdbLzUCepoc/fIm9qLw5Og2hESRVdNNrX1zunipCA7ZjIhkuVOtCtFXNo\n" \
    "2wyHcdqL9f5zEY3e2W/PcqPk9ozRQ2GCZZY90i8pcLDkSOaJ4J4BFXCvSmwMPdyg\n" \
    "VQnryl3NKxZ/g+YFJZAEiw70ycY+839FJST1VIbHxNMioFksiwR2mT0oWhLg7di7\n" \
    "bSPf5FN61/yzQDjAybJojyFSOrmjbBzMVYrMtcRrelH3TmNQd2ZntwIDAQABo4IC\n" \
    "jjCCAoowDgYDVR0PAQH/BAQDAgWgMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEF\n" \
    "BQcDAjAMBgNVHRMBAf8EAjAAMB0GA1UdDgQWBBQSSD3+m1e+sonGl/tDzR+qwXVT\n" \
    "sjAfBgNVHSMEGDAWgBSoSmpjBH3duubRObemRWXv86jsoTBvBggrBgEFBQcBAQRj\n" \
    "MGEwLgYIKwYBBQUHMAGGImh0dHA6Ly9vY3NwLmludC14My5sZXRzZW5jcnlwdC5v\n" \
    "cmcwLwYIKwYBBQUHMAKGI2h0dHA6Ly9jZXJ0LmludC14My5sZXRzZW5jcnlwdC5v\n" \
    "cmcvMEQGA1UdEQQ9MDuCDmVjaG9pY3RlY2guY29tghVyZXZlcmIuZWNob2ljdGVj\n" \
    "aC5jb22CEnd3dy5lY2hvaWN0ZWNoLmNvbTBMBgNVHSAERTBDMAgGBmeBDAECATA3\n" \
    "BgsrBgEEAYLfEwEBATAoMCYGCCsGAQUFBwIBFhpodHRwOi8vY3BzLmxldHNlbmNy\n" \
    "eXB0Lm9yZzCCAQQGCisGAQQB1nkCBAIEgfUEgfIA8AB2AG9Tdqwx8DEZ2JkApFEV\n" \
    "/3cVHBHZAsEAKQaNsgiaN9kTAAABakEuU+EAAAQDAEcwRQIhALODuzmJwfa6F3Na\n" \
    "yrilQ6pw3pm5ynImLwn5ylqeZAzGAiAEq5m7Ui8FkzaYZgC/b5PjF0uspEedAWrn\n" \
    "7TJOBnVt3QB2ACk8UZZUyDlluqpQ/FgH1Ldvv1h6KXLcpMMM9OVFR/R4AAABakEu\n" \
    "VKMAAAQDAEcwRQIgBmHnhYx5rV9tS5U/tnOPMB8DwWKdM02VSxU4cROgRhUCIQCn\n" \
    "/CeLQED72P1zfiOKYig0Vj13UBm290IaeE/zGg4y8jANBgkqhkiG9w0BAQsFAAOC\n" \
    "AQEAGDgBbCziqIZTx2fAWz5L1pYhVEKs+h1k6yJIk0IIw83Y0gMsJc8Fc37X4emD\n" \
    "Pp9iXmkAkRUwHr9S1afuARX/PlIvsMs76pcr9MHt/02xylQba/45uPI6fTlcSCMd\n" \
    "sPuQ7BNpOSwZ/H8tm4CRCCOWao2kEdWnpLB5TkZHBztjrYm7uqi7vfuiGKEPcAEc\n" \
    "exrrjdh/XubMUWo/sr1xxoNV8HTXlGtiFa0t8NTXkUNb/nVvMhtqCY2u0fog4wX/\n" \
    "7c9i8IhmlF8WT5Sy8mku2tvYdwnZwg2umUDKFUgGqo9EwfobQ5Mma6Qts6MRXzDL\n" \
    "CUa9SXHxXySWuQS9E9DyhmTjwA==\n" \
    "-----END CERTIFICATE-----\n"



// Static because the scope of this object is the usage of the iap_https module.
mach1_firmware_info_t mach1_firmware_verison = {
    .major = 0,
    .minor = 0,
    .patch = 1,
};

// typedef struct _mach1_ota_server_info_t {
//     const char* server_host_name;
//     const char* server_metadata_path;
//     const char* server_firmware_path;
// }mach1_ota_server_info_t;

// static mach1_ota_server_info_t mach1_factory_paths = {
//     .server_host_name = ,
//     .server_metadata_path = ,

//     // .server_firmware_path = // should be set by the metadata file
// };

static const iap_https_config_t mach1_factory_ota_config = {
    .current_firmware_version = &mach1_firmware_verison,
    .server_host_name = OTA_SERVER_HOST_NAME,
    .server_port = "443",
    .server_root_ca_public_key_pem = OTA_SERVER_ROOT_CA_PEM,
    .peer_public_key_pem = OTA_PEER_PEM,
    // .server_metadata_path = 
    // .server_firmware_path = 
    .polling_interval_s = 0, // manual checks
    .auto_reboot = 1,
};

static iap_https_config_t ota_config; // the ota_config that can be modified by the user
bool mach1_ota_initialized = false;
static void init_ota( void );

volatile bool mach1_initialized = false;

#define MACH1_INSIG_BRK_PIN     (34)
#define MACH1_INSIG_LTS_PIN     (35)
#define MACH1_INSIG_RTS_PIN     (36)
#define MACH1_INSIG_REV_PIN     (39)

#define MACH1_USW1_PIN          (27)
#define MACH1_USW2_PIN          (32)
#define MACH1_USW3_PIN          (19)

#define MACH1_INPUT_PINS_MASK ( \
    1ULL<<MACH1_INSIG_BRK_PIN | \
    1ULL<<MACH1_INSIG_LTS_PIN | \
    1ULL<<MACH1_INSIG_RTS_PIN | \
    1ULL<<MACH1_INSIG_REV_PIN | \
    1ULL<<MACH1_USW1_PIN | \
    1ULL<<MACH1_USW2_PIN | \
    1ULL<<MACH1_USW3_PIN \
    )

#define ESP_INTR_FLAG_DEFAULT 0

#define MACH1_COND_RISING_BRK   (0x1C1B)
#define MACH1_COND_RISING_LTS   (0x1C1C)
#define MACH1_COND_RISING_RTS   (0x1C1D)
#define MACH1_COND_RISING_REV   (0x1C1E)

#define MACH1_COND_FALLING_BRK  (0x1C0B)
#define MACH1_COND_FALLING_LTS  (0x1C0C)
#define MACH1_COND_FALLING_RTS  (0x1C0D)
#define MACH1_COND_FALLING_REV  (0x1C0E)

#define MACH1_COND_RISING_USW1   (0x1C11)
#define MACH1_COND_RISING_USW2   (0x1C12)
#define MACH1_COND_RISING_USW3   (0x1C13)

#define MACH1_COND_FALLING_USW1   (0x1C01)
#define MACH1_COND_FALLING_USW2   (0x1C02)
#define MACH1_COND_FALLING_USW3   (0x1C03)

typedef struct _m1_input_states_t{
    bool brk;
    bool lts;
    bool rts;
    bool rev;
    bool sw1;
    bool sw2;
    bool sw3;
    int64_t timestamp;
}m1_input_states_t;

volatile m1_input_states_t m1_input_state_previous;
volatile m1_input_states_t m1_input_state_current = {   // initialize the current state to reflect the default (pull up/down) states of these inputs
    .brk = false,
    .lts = false,
    .rts = false,
    .rev = false,
    .sw1 = true,
    .sw2 = true,
    .sw3 = true,
    .timestamp = 0,
};

// // conditions:
// const uint32_t m1_cond_brake_rising

volatile bool isr_fired = false;

extern mp_obj_t multipython_notify(mp_obj_t condition);
IRAM_ATTR void mach1_gpio_input_isr( void*  args ){
    // sample all the inputs and notify of any applicable conditions

    int64_t current_time = esp_timer_get_time();

    // with this being a 64-bit variable I just won't worry about overflow...
    if( (current_time - m1_input_state_previous.timestamp) > (60*1000) ){

        isr_fired = true;

        m1_input_state_previous = m1_input_state_current;
        m1_input_state_current.brk = gpio_get_level(MACH1_INSIG_BRK_PIN);
        m1_input_state_current.lts = gpio_get_level(MACH1_INSIG_LTS_PIN);
        m1_input_state_current.rts = gpio_get_level(MACH1_INSIG_RTS_PIN);
        m1_input_state_current.rev = gpio_get_level(MACH1_INSIG_REV_PIN);
        m1_input_state_current.sw1 = gpio_get_level(MACH1_USW1_PIN);
        m1_input_state_current.sw2 = gpio_get_level(MACH1_USW2_PIN);
        m1_input_state_current.sw3 = gpio_get_level(MACH1_USW3_PIN);
        m1_input_state_current.timestamp = current_time;

        // check for changes
        if( m1_input_state_current.brk != m1_input_state_previous.brk ){
            if( m1_input_state_current.brk ){ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_RISING_BRK)); }
            else{ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_FALLING_BRK)); }
        }
        if( m1_input_state_current.lts != m1_input_state_previous.lts ){
            if( m1_input_state_current.lts ){ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_RISING_LTS)); }
            else{ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_FALLING_LTS)); }
        }
        if( m1_input_state_current.rts != m1_input_state_previous.rts ){
            if( m1_input_state_current.rts ){ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_RISING_RTS)); }
            else{ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_FALLING_RTS)); }
        }
        if( m1_input_state_current.rev != m1_input_state_previous.rev ){
            if( m1_input_state_current.rev ){ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_RISING_REV)); }
            else{ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_FALLING_REV)); }
        }

        if( m1_input_state_current.sw1 != m1_input_state_previous.sw1 ){
            if( m1_input_state_current.sw1 ){ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_RISING_USW1)); }
            else{ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_FALLING_USW1)); }
        }
        if( m1_input_state_current.sw2 != m1_input_state_previous.sw2 ){
            if( m1_input_state_current.sw2 ){ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_RISING_USW2)); }
            else{ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_FALLING_USW2)); }
        }
        if( m1_input_state_current.sw3 != m1_input_state_previous.sw3 ){
            if( m1_input_state_current.sw3 ){ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_RISING_USW3)); }
            else{ multipython_notify(MP_OBJ_NEW_SMALL_INT(MACH1_COND_FALLING_USW3)); }
        }
    }
}

mp_obj_t mach1__boot( void ){
    // This is called from _boot.py to initialize the Mach1 board
    if( mach1_initialized ){ return mp_const_none; }

    esp_err_t retval = ESP_OK;

    // Set up input signals
    gpio_config_t insig_config = {0};
    insig_config.pin_bit_mask = MACH1_INPUT_PINS_MASK;
    insig_config.mode = GPIO_MODE_INPUT;
    insig_config.pull_up_en = 0;
    insig_config.pull_down_en = 0;
    insig_config.intr_type = GPIO_INTR_ANYEDGE;

    retval = gpio_config(&insig_config);
    if( retval != ESP_OK ){
        printf("error configuring input pins (%d)\n", retval);
    }

    retval = gpio_isr_handler_add(MACH1_INSIG_BRK_PIN, mach1_gpio_input_isr, NULL);
    if( retval != ESP_OK ){
        printf("error registering input ISR for pin %d (%d)\n",MACH1_INSIG_BRK_PIN, retval);
    }

    gpio_isr_handler_add(MACH1_INSIG_LTS_PIN, mach1_gpio_input_isr, NULL);
    gpio_isr_handler_add(MACH1_INSIG_RTS_PIN, mach1_gpio_input_isr, NULL);
    gpio_isr_handler_add(MACH1_INSIG_REV_PIN, mach1_gpio_input_isr, NULL);

    gpio_isr_handler_add(MACH1_USW1_PIN, mach1_gpio_input_isr, NULL);
    gpio_isr_handler_add(MACH1_USW2_PIN, mach1_gpio_input_isr, NULL);
    gpio_isr_handler_add(MACH1_USW3_PIN, mach1_gpio_input_isr, NULL);

    // while(1){
    //     if( isr_fired ){
    //         printf("isr: %d, %d, %d, %d, %d, %d, %d, time difference: 0x%08X%08X\n",m1_input_state_current.brk, m1_input_state_current.lts, m1_input_state_current.rts, m1_input_state_current.rev, m1_input_state_current.sw1, m1_input_state_current.sw2, m1_input_state_current.sw3, (uint32_t)((m1_input_state_current.timestamp - m1_input_state_previous.timestamp) >> 32), (uint32_t)((m1_input_state_current.timestamp - m1_input_state_previous.timestamp) & 0xFFFFFFFF));
    //         vTaskDelay(50/portTICK_PERIOD_MS);
    //         isr_fired = false;
    //     }
    // }

    mach1_initialized = true;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mach1__boot_obj, mach1__boot);


// interface 
STATIC mp_obj_t mach1_system(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args){

#if CONFIG_SPIRAM_SUPPORT
    switch (esp_spiram_get_chip_size()) {
        case ESP_SPIRAM_SIZE_16MBITS:
            printf("spiram is 2MB\n");
            break;
        case ESP_SPIRAM_SIZE_32MBITS:
            printf("spiram is 4MB\n");
            break;
        case ESP_SPIRAM_SIZE_64MBITS:
            printf("spiram is 8MB\n");
            break;
        default:
            printf("no spiram\n");
            break;
    }
#endif

    if( heap_caps_check_integrity_all(true) ){
        printf("heap caps integrity ok\n");
    }else{
        printf("heap caps integrity ok\n");
    }

    printf("free memory: 0x%X, high water mark (lowest free memory all time): 0x%X\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size() );

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach1_system_obj, 0, mach1_system);



STATIC mp_obj_t mach1_firmware(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args){
    enum {  ARG_check_updates, 
            ARG_install_updates, 
            ARG_peer_certificate, 
            ARG_root_certificate,
            ARG_host_url,
            ARG_metadata_url,
            ARG_use_factory_update  };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_check_updates,        MP_ARG_OBJ,     {.u_obj = mp_const_false} },
        { MP_QSTR_install_updates,      MP_ARG_OBJ,     {.u_obj = mp_const_false} },
        { MP_QSTR_peer_certificate,     MP_ARG_OBJ,     {.u_obj = mp_const_none} },
        { MP_QSTR_root_certificate,     MP_ARG_OBJ,     {.u_obj = mp_const_none} },
        { MP_QSTR_host_url,             MP_ARG_OBJ,     {.u_obj = mp_const_none} },
        { MP_QSTR_metadata_url,         MP_ARG_OBJ,     {.u_obj = mp_const_none} },
        { MP_QSTR_use_factory_update,   MP_ARG_OBJ,     {.u_obj = mp_const_false} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    printf("Mach1 Firmware version: %d.%d.%d\n", mach1_firmware_verison.major, mach1_firmware_verison.minor, mach1_firmware_verison.patch );

    if( !mach1_ota_initialized ){
        // Do what you need to do to initialize ota
        init_wifi_sta_event_group();
        init_ota();

        mach1_ota_initialized = true;
    }

    // First, if the user has specified any special settings use those:
    if( mp_obj_is_true(args[ARG_peer_certificate].u_obj) ){
        if( !mp_obj_is_str( args[ARG_peer_certificate].u_obj ) ){
            printf("error: string expected for 'peer_certificate.' using previously set certificate\n");
        }else{
            printf("error: custom peer certificate specification not yet supported\n");
            // const char *mp_obj_str_get_data(mp_obj_t self_in, size_t *len);
        }
    }

    if( mp_obj_is_true(args[ARG_root_certificate].u_obj) ){
        if( !mp_obj_is_str( args[ARG_root_certificate].u_obj ) ){
            printf("error: string expected for 'root_certificate.' using previously set certificate\n");
        }else{
            printf("error: custom root certificate specification not yet supported\n");
            // const char *mp_obj_str_get_data(mp_obj_t self_in, size_t *len);
        }
    }

    if( mp_obj_is_true(args[ARG_host_url].u_obj) ){
        if( !mp_obj_is_str( args[ARG_host_url].u_obj ) ){
            printf("error: string expected for 'image_url.' using previously set certificate\n");
        }else{
            printf("error: image URL specification not yet supported\n");
            // const char *mp_obj_str_get_data(mp_obj_t self_in, size_t *len);
        }
    }

    if( mp_obj_is_true(args[ARG_metadata_url].u_obj) ){
        if( !mp_obj_is_str( args[ARG_metadata_url].u_obj ) ){
            printf("error: string expected for 'metadata_url.' using previously set certificate\n");
        }else{
            printf("error: metadata URL specification not yet supported\n");
            // const char *mp_obj_str_get_data(mp_obj_t self_in, size_t *len);
        }
    }


    // Then in case the user has also specified to use the factory settings switch to those...
    if( mp_obj_is_true(args[ARG_peer_certificate].u_obj) ){
        printf("error: using factory settings is currently unsupported\n");
    }

    // Now do the stuff that they've asked
    if( mp_obj_is_true(args[ARG_check_updates].u_obj) ){
        // printf("error: checking for updates not yet supported\n");
        mach1_firmware_info_t server_version;
        uint8_t error = 0;
        iap_https_get_server_version( &server_version.major, &server_version.minor, &server_version.patch, &error );
        if( error == 0 ){
            printf("Server version number: %d.%d.%d\n", server_version.major, server_version.minor, server_version.patch );
        }else{
            printf("server version number could not be checked\n");
        }
    }

    if( mp_obj_is_true(args[ARG_install_updates].u_obj) ){
        // printf("error: installing updates not yet supported\n");
        mach1_firmware_info_t server_version;
        uint8_t error = 0;
        iap_https_get_server_version( &server_version.major, &server_version.minor, &server_version.patch, &error );
        if( error == 0 ){
            printf("Server version number: %d.%d.%d\n", server_version.major, server_version.minor, server_version.patch );
            printf("Current version number: %d.%d.%d\n", mach1_firmware_verison.major, mach1_firmware_verison.minor, mach1_firmware_verison.patch );
            
            if( ( server_version.major == mach1_firmware_verison.major ) && 
                ( server_version.minor == mach1_firmware_verison.minor ) && 
                ( server_version.patch == mach1_firmware_verison.patch ) ){
                printf("Firmware version is already up-to-date.\n");
            }else{
                printf("Beginning firmware upgrade - do not turn off or leave WiFi until complete\n");
                // Do the thing
                iap_https_install_server_image();
            }
        }else{
            printf("Could not confirm firmware version on server.\n");
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach1_firmware_obj, 0, mach1_firmware);

STATIC mp_obj_t mach1_input_states( void ){
    mp_obj_t input_states = mp_obj_new_dict(7);

    const char* str_key_brk = "BRK";
    const char* str_key_lts = "LTS";
    const char* str_key_rts = "RTS";
    const char* str_key_rev = "REV";
    const char* str_key_usw1 = "USW1";
    const char* str_key_usw2 = "USW2";
    const char* str_key_usw3 = "USW3";

    mp_obj_t key_brk = mp_obj_new_str_via_qstr(str_key_brk, strlen(str_key_brk));
    mp_obj_t key_lts = mp_obj_new_str_via_qstr(str_key_lts, strlen(str_key_lts));
    mp_obj_t key_rts = mp_obj_new_str_via_qstr(str_key_rts, strlen(str_key_rts));
    mp_obj_t key_rev = mp_obj_new_str_via_qstr(str_key_rev, strlen(str_key_rev));
    mp_obj_t key_usw1 = mp_obj_new_str_via_qstr(str_key_usw1, strlen(str_key_usw1));
    mp_obj_t key_usw2 = mp_obj_new_str_via_qstr(str_key_usw2, strlen(str_key_usw2));
    mp_obj_t key_usw3 = mp_obj_new_str_via_qstr(str_key_usw3, strlen(str_key_usw3));

    mp_obj_t brk = mp_obj_new_int((mp_int_t)gpio_get_level(MACH1_INSIG_BRK_PIN));
    mp_obj_t lts = mp_obj_new_int((mp_int_t)gpio_get_level(MACH1_INSIG_LTS_PIN));
    mp_obj_t rts = mp_obj_new_int((mp_int_t)gpio_get_level(MACH1_INSIG_RTS_PIN));
    mp_obj_t rev = mp_obj_new_int((mp_int_t)gpio_get_level(MACH1_INSIG_REV_PIN));
    mp_obj_t usw1 = mp_obj_new_int((mp_int_t)gpio_get_level(MACH1_USW1_PIN));
    mp_obj_t usw2 = mp_obj_new_int((mp_int_t)gpio_get_level(MACH1_USW2_PIN));
    mp_obj_t usw3 = mp_obj_new_int((mp_int_t)gpio_get_level(MACH1_USW3_PIN));

    mp_obj_dict_store( input_states,    key_brk,    brk     );
    mp_obj_dict_store( input_states,    key_lts,    lts     );
    mp_obj_dict_store( input_states,    key_rts,    rts     );
    mp_obj_dict_store( input_states,    key_rev,    rev     );
    mp_obj_dict_store( input_states,    key_usw1,   usw1    );
    mp_obj_dict_store( input_states,    key_usw2,   usw2    );
    mp_obj_dict_store( input_states,    key_usw3,   usw3    );

    return input_states;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mach1_input_states_obj, mach1_input_states);

STATIC const mp_rom_map_elem_t mp_module_mach1_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_mach1) },
    { MP_ROM_QSTR(MP_QSTR__boot), MP_ROM_PTR(&mach1__boot_obj) },
    { MP_ROM_QSTR(MP_QSTR_system), MP_ROM_PTR(&mach1_system_obj) },
    { MP_ROM_QSTR(MP_QSTR_firmware), MP_ROM_PTR(&mach1_firmware_obj) },
    { MP_ROM_QSTR(MP_QSTR_input_states), MP_ROM_PTR(&mach1_input_states_obj) },

    { MP_ROM_QSTR(MP_QSTR_RISING_BRK), MP_ROM_INT(MACH1_COND_RISING_BRK) },
    { MP_ROM_QSTR(MP_QSTR_RISING_LTS), MP_ROM_INT(MACH1_COND_RISING_LTS) },
    { MP_ROM_QSTR(MP_QSTR_RISING_RTS), MP_ROM_INT(MACH1_COND_RISING_RTS) },
    { MP_ROM_QSTR(MP_QSTR_RISING_REV), MP_ROM_INT(MACH1_COND_RISING_REV) },

    { MP_ROM_QSTR(MP_QSTR_FALLING_BRK), MP_ROM_INT(MACH1_COND_FALLING_BRK) },
    { MP_ROM_QSTR(MP_QSTR_FALLING_LTS), MP_ROM_INT(MACH1_COND_FALLING_LTS) },
    { MP_ROM_QSTR(MP_QSTR_FALLING_RTS), MP_ROM_INT(MACH1_COND_FALLING_RTS) },
    { MP_ROM_QSTR(MP_QSTR_FALLING_REV), MP_ROM_INT(MACH1_COND_FALLING_REV) },

    { MP_ROM_QSTR(MP_QSTR_RISING_USW1), MP_ROM_INT(MACH1_COND_RISING_USW1) },
    { MP_ROM_QSTR(MP_QSTR_RISING_USW2), MP_ROM_INT(MACH1_COND_RISING_USW2) },
    { MP_ROM_QSTR(MP_QSTR_RISING_USW3), MP_ROM_INT(MACH1_COND_RISING_USW3) },

    { MP_ROM_QSTR(MP_QSTR_FALLING_USW1), MP_ROM_INT(MACH1_COND_FALLING_USW1) },
    { MP_ROM_QSTR(MP_QSTR_FALLING_USW2), MP_ROM_INT(MACH1_COND_FALLING_USW2) },
    { MP_ROM_QSTR(MP_QSTR_FALLING_USW3), MP_ROM_INT(MACH1_COND_FALLING_USW3) },
};
STATIC MP_DEFINE_CONST_DICT(mp_module_mach1_globals, mp_module_mach1_globals_table);

const mp_obj_module_t mp_module_mach1 = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_mach1_globals,
};








static void init_ota()
{
    ESP_LOGI(TAG, "Initialising OTA firmware updating.");
    
    ota_config = mach1_factory_ota_config; // copy the factory settings

    // ota_config.current_firmware_version = &mach1_firmware_verison;
    // ota_config.server_host_name = OTA_SERVER_HOST_NAME;
    // ota_config.server_port = "443";
    strncpy(ota_config.server_metadata_path, OTA_SERVER_METADATA_PATH, sizeof(ota_config.server_metadata_path) / sizeof(char));
    bzero(ota_config.server_firmware_path, sizeof(ota_config.server_firmware_path) / sizeof(char));
    // ota_config.server_root_ca_public_key_pem = server_root_ca_public_key_pem;
    // ota_config.peer_public_key_pem = peer_public_key_pem;
    // ota_config.polling_interval_s = OTA_POLLING_INTERVAL_S;
    // ota_config.auto_reboot = OTA_AUTO_REBOOT;
    
    iap_https_init(&ota_config);
    
    // // Immediately check if there's a new firmware image available.
    // iap_https_check_now();
}