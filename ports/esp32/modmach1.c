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


#include "ota/wifi_sta.h"   // WIFI module configuration, connecting to an access point.
#include "ota/iap_https.h"  // Coordinating firmware updates



#define TAG "Mach1"


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



// interface 
STATIC mp_obj_t mach1_system(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args){
    // start new processes (contexts)


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



STATIC const mp_rom_map_elem_t mp_module_mach1_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_mach1) },
    { MP_ROM_QSTR(MP_QSTR_system), MP_ROM_PTR(&mach1_system_obj) },
    { MP_ROM_QSTR(MP_QSTR_firmware), MP_ROM_PTR(&mach1_firmware_obj) },
    // { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&multipython_start_obj) },
    // { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&multipython_stop_obj) },
    // { MP_ROM_QSTR(MP_QSTR_suspend), MP_ROM_PTR(&multipython_suspend_obj) },
    // { MP_ROM_QSTR(MP_QSTR_resume), MP_ROM_PTR(&multipython_resume_obj) },
    // { MP_ROM_QSTR(MP_QSTR_get), MP_ROM_PTR(&multipython_get_obj) },
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