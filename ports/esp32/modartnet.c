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

/*

The artnet module is designed to facilitate processing of ArtNet packets

*/

// #include "py/binary.h"
// #include "py/builtin.h"
// #include "py/compile.h"
#include "py/mperrno.h"
// #include "py/gc.h"
#include "py/objmodule.h"
// #include "py/mphal.h"
// #include "py/mpstate.h"
// #include "py/nlr.h"
#include "py/obj.h"
// #include "py/parse.h"
#include "py/runtime.h"
// #include "py/stackctrl.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "modartnet.h"


// todo: figure out if all the strings are taking up ram, flash, etc - then decide if we need to do anything
// todo: add some way for the user to interact with the received data (i.e. through Python AND through c directly)

// e.g. a class that directly exposes the universe, data, and a callback when new data is received
// and for c espose a similar structure... and maybe allow for a linked list of callback functions


// #define ARTNET_ITER_FROM_CALLBACK_PTR(ptr) ((artnet_callback_node_iter_t)ptr)
// #define ARTNET_CALLBACK_PTR_FROM_ITER(iter) ((artnet_callback_node_t*)iter)


volatile artnet_packet_t    rx_artnet_packet; // todo: make sure this is in RAM?
// artnet_callback_node_t*     artnet_callbacks_head;
xTaskHandle                 artnet_task_handle;
static int                  artnet_initialized = 0;
static const char*          TAG = "modartnet";

addressable_layer_artdmx_info_node_t*   artdmx_info_head = NULL;    // head of the LL of artdmx info nodes, for any layer

// typedef artnet_callback_node_t* artnet_callback_node_iter_t;



// forward declarations
STATIC mp_obj_t artnet_get_network_interface_active_fn( mp_obj_t interface );
STATIC mp_obj_t artnet_network_interface_active( mp_obj_t interface_active_fn );
STATIC mp_obj_t artnet_interface_activate( mp_obj_t interface_active_fn );
// void artnet_call_callback_helper( artnet_callback_node_iter_t iter, void* args );
// void artnet_callback_foreach(artnet_callback_node_iter_t head, void (*f)(artnet_callback_node_iter_t iter, void*), void* args);
static void artnet_server_task(void *pvParameters);


// void artnet_debug_callback_helper( artnet_callback_node_iter_t iter, void* args );




// int8_t artnet_add_callback_c_args( artnet_callback_args_t cb, void* args ){
//     if( cb == NULL ){ return -1; }
//     printf("Here??\n" ); 
//     artnet_callback_node_t* node = artnet_append_callback_node();
//     if( node == NULL ){ return -1; }
//     printf("Adding callback 0x%08X\n", (uint32_t)cb ); 
//     node->c_callback = cb;
//     node->args = args;

// // int artnet_remove_callback_node( artnet_callback_node_t* node );
//     return 0;
// }

// int8_t artnet_add_callback_c( artnet_callback_t cb ){
//     return artnet_add_callback_c_args( (artnet_callback_args_t)cb, NULL ); // todo: is this kind of thing legal??? calling conventions?
// }



// interface 
mp_obj_t artnet_start( mp_obj_t interface ){ // todo: add flexibility for specifying network and password for STA interfac

    // todo: allow specification of SSID and password

    mp_printf(&mp_plat_print, "Size of artnet packet: %d\n", sizeof(artnet_packet_t)); 

    if( artnet_initialized ){ 
        mp_printf(&mp_plat_print, "ArtNet already active\n"); 
        return mp_const_none;
    }

    mp_obj_t intfc_active_fn = artnet_get_network_interface_active_fn( interface );
    if( !intfc_active_fn ){
        mp_raise_OSError(MP_ENOENT);
        return mp_const_none;
    }

    if( !mp_obj_is_true(artnet_network_interface_active(intfc_active_fn)) ){
        mp_obj_t active = artnet_interface_activate(intfc_active_fn);
        if( !mp_obj_is_true(active) ){
            mp_raise_OSError(MP_ENOENT);
            return mp_const_none;
        }
    }

    // ESP_ERROR_CHECK( nvs_flash_init() );
    // initialise_wifi();
    // wait_for_ip();
    xTaskCreate(artnet_server_task, "artnet upd task", 4096, NULL, 9, &artnet_task_handle); //todo: optimize memory usage, and or allow user to specify it

    if( artnet_task_handle != NULL ){
        artnet_initialized = 1;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(artnet_start_obj, artnet_start);

mp_obj_t artnet_stop( void ) {
    // start new processes (contexts)

    if( !artnet_initialized ){
        mp_printf(&mp_plat_print, "ArtNet already stopped\n"); 
        return mp_const_none;
    }

    if( artnet_task_handle == NULL ){
        mp_printf(&mp_plat_print, "ArtNet invalid state\n"); 
        artnet_initialized = 0;
        return mp_const_none;
    }

    mp_printf(&mp_plat_print, "Stopping ArtNet\n"); 

    // todo: What (else?) do we need to do to stop the udp server stuff?

    // todo: need to unbind the socket, so that if we try to re-start artnet we avoid the Socket unable to bind: errno 112 (Address already in use) error

    vTaskDelete(artnet_task_handle);
    artnet_initialized = 0;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(artnet_stop_obj, artnet_stop);

// mp_obj_t artnet_add_callback( mp_obj_t callback ){
//     if( !callback ){ return mp_const_none; }
//     mp_printf(&mp_plat_print, "Here??\n" ); 
//     artnet_callback_node_t* node = artnet_append_callback_node();
//     if( node == NULL ){ return mp_const_none; }
//     mp_printf(&mp_plat_print, "Adding callback 0x%08X\n", (uint32_t)node ); 
//     node->p_callback = callback;
//     node->p_context = mp_active_contexts[MICROPY_GET_CORE_INDEX]; // store the context from which this callback was registered

// // int artnet_remove_callback_node( artnet_callback_node_t* node );
//     return callback;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_1(artnet_add_callback_obj, artnet_add_callback);

// mp_obj_t artnet_show_callbacks( void ){
//     // artnet_callback_foreach( artnet_callbacks_head, artnet_call_callback_helper, NULL );
//     artnet_callback_foreach( artnet_callbacks_head, artnet_debug_callback_helper, NULL );
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_0(artnet_show_callbacks_obj, artnet_show_callbacks);

// mp_obj_t artnet_call_callbacks( void ){
//     artnet_callback_foreach( artnet_callbacks_head, artnet_call_callback_helper, NULL );
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_0(artnet_call_callbacks_obj, artnet_call_callbacks);


// Module Definitions
STATIC const mp_rom_map_elem_t mp_module_artnet_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_artnet) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&artnet_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&artnet_stop_obj) },

    { MP_ROM_QSTR(MP_QSTR_new_layer_artdmx_info),  MP_ROM_PTR(&addressable_layer_artdmx_infoObj_type) },
    // { MP_ROM_QSTR(MP_QSTR_add_callback), MP_ROM_PTR(&artnet_add_callback_obj) },
    // { MP_ROM_QSTR(MP_QSTR_show_callbacks), MP_ROM_PTR(&artnet_show_callbacks_obj) },
    // { MP_ROM_QSTR(MP_QSTR_call_callbacks), MP_ROM_PTR(&artnet_call_callbacks_obj) },
};
STATIC MP_DEFINE_CONST_DICT(mp_module_artnet_globals, mp_module_artnet_globals_table);

const mp_obj_module_t mp_module_artnet = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_artnet_globals,
};



// #define EXAMPLE_WIFI_SSID "esp_net"// CONFIG_WIFI_SSID              // Now we just need to make this irrespective of the station, so that it can be an access point too
// #define EXAMPLE_WIFI_PASS "artnet88"//CONFIG_WIFI_PASSWORD

#define PORT 6454//CONFIG_EXAMPLE_PORT // ART_NET_PORT 6454
#define EXAMPLE_MAX_STA_CONN 5// could we do more?




static void artnet_server_task(void *pvParameters)
{
    // char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (1) {

#ifdef CONFIG_EXAMPLE_IPV4
        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else // IPV6
        struct sockaddr_in6 destAddr;
        bzero(&destAddr.sin6_addr.un, sizeof(destAddr.sin6_addr.un));
        destAddr.sin6_family = AF_INET6;
        destAddr.sin6_port = htons(PORT);
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
        inet6_ntoa_r(destAddr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err = bind(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAG, "Socket binded");

        while (1) {

            // ESP_LOGI(TAG, "Waiting for data");
            struct sockaddr_in6 sourceAddr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(sourceAddr);
            int len = recvfrom(sock, (void*)&rx_artnet_packet, sizeof(rx_artnet_packet), 0, (struct sockaddr *)&sourceAddr, &socklen);

            // Error occured during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (sourceAddr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&sourceAddr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (sourceAddr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(sourceAddr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                // Check the artnet packet header to ensure that the packet is valid:
                if( strcmp((const char*)&rx_artnet_packet.ID, "Art-Net") != 0 ){
                    // ESP_LOGI(TAG, "ArtNet packet ID failed");
                    continue; // bail b/c ID does not match Art-Net ID
                }

                // Check the op-code to ensure this is a ArtDMX packet:
                if( ((rx_artnet_packet.OpCode.OpCodeBytes.OpCodeHi << 8) | (rx_artnet_packet.OpCode.OpCodeBytes.OpCodeLo & 0xFF)) != 0x5000 ){
                    // ESP_LOGI(TAG, "ArtNet ArtDMX OpCode failed");
                    continue; // bail b/c this is not good ArtDMX data
                }

                // Check the protocol version:
                if( ((rx_artnet_packet.ProtVer.ProtVerBytes.ProtVerHi << 8) | (rx_artnet_packet.ProtVer.ProtVerBytes.ProtVerLo & 0xFF)) < 14 ){
                    // ESP_LOGI(TAG, "ArtNet ProtVer failed");
                    continue; // bail w/o notifying application of new data
                }

                // The following fields are not checked:
                // Sequence
                // Physical
                // Port Address (Net and SubUni)

                // Check Length
                uint16_t rxPacketLength = ((rx_artnet_packet.Length.LengthBytes.LengthHi << 8) | (rx_artnet_packet.Length.LengthBytes.LengthLo & 0xFF));
                if( rxPacketLength > 512 ){
                    // ESP_LOGI(TAG, "ArtNet ArtDMX Length failed");
                    continue; // bail w/o notifying application of new data 
                }

                uint16_t portAddress = ((rx_artnet_packet.Net<<8) | (rx_artnet_packet.SubUni & 0xFF));
                // printf("we got a valid packet! portAddress = 0x%08X\n", ((rx_artnet_packet.Net<<8) | (rx_artnet_packet.SubUni & 0xFF)) );

                // If we've made it here distribute this data to any linked fixture layers:
                // That means searching through all layers of all fixtures... wait that's not efficient! Grr...
                // okay I've changed it so that all artdmx info nodes get put into one LL with the head at artdmx_info_head
                addressable_layer_artdmx_info_node_iter_t iter = NULL;
                for( iter = addressable_layer_artdmx_info_first(artdmx_info_head); !addressable_layer_artdmx_info_done(iter); iter = addressable_layer_artdmx_info_next(iter) ){ 
                    
                    // printf("\tchecking dmx info at 0x%08X: portAddress = 0x%08X, layer = 0x%08X\n", (uint32_t)iter, (uint32_t)ADDRESSABLE_LAYER_ARTDMX_INFO_PTR_FROM_ITER(iter)->artdmx_info->portAddress, (uint32_t)ADDRESSABLE_LAYER_ARTDMX_INFO_PTR_FROM_ITER(iter)->artdmx_info->layer );

                    if(ADDRESSABLE_LAYER_ARTDMX_INFO_PTR_FROM_ITER(iter)->artdmx_info->portAddress == portAddress){
                        // If the port address matches then try to put led data into the layer data
                        addressable_layer_obj_t* layer = ADDRESSABLE_LAYER_ARTDMX_INFO_PTR_FROM_ITER(iter)->artdmx_info->layer;


                        if(layer == NULL){ continue; } // bail out b/c invalid layer
                        uint8_t cpl = ADDRESSABLE_LAYER_ARTDMX_INFO_PTR_FROM_ITER(iter)->artdmx_info->cpl;
                        uint16_t start_channel = ADDRESSABLE_LAYER_ARTDMX_INFO_PTR_FROM_ITER(iter)->artdmx_info->start_channel;
                        uint16_t max_channels = 512 - start_channel;
                        uint16_t num_leds = ADDRESSABLE_LAYER_ARTDMX_INFO_PTR_FROM_ITER(iter)->artdmx_info->leds;
                        uint16_t start_led = ADDRESSABLE_LAYER_ARTDMX_INFO_PTR_FROM_ITER(iter)->artdmx_info->start_index;
                        uint16_t num_channels = num_leds*cpl;
                        if( num_channels > rxPacketLength ){
                            num_channels = rxPacketLength;
                        }
                        num_leds = num_channels/cpl; // integer division to find the new number of leds, which won't violate the DMX packet size

                        // printf("\t\tgot a match! layer = 0x%08X, cpl = %d, num_leds = %d\n", (uint32_t)layer, cpl, num_leds);

                        if( cpl != 3 && cpl !=4 ){ continue; } // bail out for unrecognized format (RGB or RGBA only!)

                        // printf("\t\t\tmih!\n");

                        // copy the data into the layer
                        uint8_t* dst = layer->data;
                        uint8_t* src = (uint8_t*)rx_artnet_packet.Data;
                        for(uint16_t indi = 0; indi < num_leds; indi++){
                            *(dst + (MODADD_BPL*(indi + start_led) + 0)) = *(src + (cpl * indi) + start_channel + 0);
                            *(dst + (MODADD_BPL*(indi + start_led) + 1)) = *(src + (cpl * indi) + start_channel + 1);
                            *(dst + (MODADD_BPL*(indi + start_led) + 2)) = *(src + (cpl * indi) + start_channel + 2);
                            if( cpl > 3 ){
                                *(dst + (MODADD_BPL*(indi + start_led) + 3)) = *(src + (cpl * indi) + start_channel + 3);
                            }else{
                                *(dst + (MODADD_BPL*(indi + start_led) + 3)) = 0; // for now default to fully transparent
                            }
                        }
                    }
                }




                // // Call callbacks!
                // artnet_callback_foreach( artnet_callbacks_head, artnet_call_callback_helper, NULL );



                // rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                // ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                // ESP_LOGI(TAG, "%s", packet);

                // uint32_t stop = 50;
                // if(len < stop){
                //     stop = len;
                // }
                // for(uint32_t indi=0; indi< stop; indi++){
                //     printf("%2X, ", *(((uint8_t*)&packet) + indi) );
                // }
                // printf("\n");

            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}





// helpers
STATIC mp_obj_t artnet_get_network_interface_active_fn( mp_obj_t interface ){
    mp_obj_t network_module_obj = mp_module_get(MP_QSTR_network);
    if( !network_module_obj ){
        mp_raise_OSError(MP_ENOENT);
        return mp_const_none;
    }

    if( (!mp_obj_is_int(interface)) || ( (mp_obj_int_get_truncated(interface) != mp_obj_int_get_truncated(mp_load_attr(network_module_obj, MP_QSTR_AP_IF))) && ((mp_obj_int_get_truncated(interface) != mp_obj_int_get_truncated(mp_load_attr(network_module_obj, MP_QSTR_STA_IF)))) ) ){
        mp_raise_TypeError("expected an integer corresonding to either network.AP_IF or network.STA_IF");
        return mp_const_none;
    }

    mp_obj_t network_WLAN_fn = mp_load_attr(network_module_obj, MP_QSTR_WLAN);
    if( !network_WLAN_fn ){
        mp_printf(&mp_plat_print, "Can't find WLAN function\n"); 
        mp_raise_OSError(MP_ENOENT);
        return mp_const_none;
    }

    mp_obj_t network_WLAN_obj = mp_call_function_1(network_WLAN_fn, interface);
    if( !network_WLAN_obj ){
        mp_printf(&mp_plat_print, "Can't find WLAN object\n"); 
        mp_raise_OSError(MP_ENOENT);
        return mp_const_none;
    }

    mp_obj_t network_interface_active_fn = mp_load_attr(network_WLAN_obj, MP_QSTR_active);
    if( !network_interface_active_fn ){
        mp_printf(&mp_plat_print, "Can't find interface object's 'active' function\n"); 
        mp_raise_OSError(MP_ENOENT);
        return mp_const_none;
    }

    return network_interface_active_fn;
}

STATIC mp_obj_t artnet_network_interface_active( mp_obj_t interface_active_fn ){
    if( interface_active_fn == mp_const_none ){
        mp_raise_OSError(MP_ENOENT);
        return mp_const_none;
    }

    mp_obj_t intfc_active = mp_call_function_0(interface_active_fn);
    if( !intfc_active ){
        mp_printf(&mp_plat_print, "No result for .active()\n"); 
        mp_raise_OSError(MP_ENOENT);
        return mp_const_none;
    }

    if(mp_obj_is_true(intfc_active)){
        return mp_const_true;
    }else{
        return mp_const_false;
    }
}

STATIC mp_obj_t artnet_interface_activate( mp_obj_t interface_active_fn ){
    if( interface_active_fn == mp_const_none ){
        mp_raise_OSError(MP_ENOENT);
        return mp_const_none;
    }

    mp_call_function_1(interface_active_fn, mp_const_true);

    mp_obj_t intfc_active = mp_call_function_0(interface_active_fn);
    if( !intfc_active ){
        mp_printf(&mp_plat_print, "Not sure why that didn't work the second time...\n"); 
        mp_raise_OSError(MP_ENOENT);
        return mp_const_false;
    }

    if( !mp_obj_is_true(intfc_active) ){
        mp_raise_OSError(MP_ENOENT);
    }
    return intfc_active;
}










// STATIC mp_obj_t addressable_layer_mode(mp_obj_t self_in, mp_obj_t mode);
// MP_DEFINE_CONST_FUN_OBJ_2(addressable_layer_mode_obj, addressable_layer_mode);

// STATIC mp_obj_t addressable_layer_set(mp_obj_t self_in, mp_obj_t start_index_obj, mp_obj_t colors);
// MP_DEFINE_CONST_FUN_OBJ_3(addressable_layer_set_obj, addressable_layer_set);

STATIC void addressable_layer_artdmx_info_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind );

STATIC const mp_rom_map_elem_t addressable_layer_artdmx_info_locals_dict_table[] = {
    // { MP_ROM_QSTR(MP_QSTR_mode), MP_ROM_PTR(&addressable_layer_mode_obj) },
    // { MP_ROM_QSTR(MP_QSTR_set), MP_ROM_PTR(&addressable_layer_set_obj) },
 };
STATIC MP_DEFINE_CONST_DICT(addressable_layer_artdmx_info_locals_dict, addressable_layer_artdmx_info_locals_dict_table);

// define the layer artdmx info class-object
const mp_obj_type_t addressable_layer_artdmx_infoObj_type = {
    { &mp_type_type },                                                          // "inherit" the type "type"
    .name = MP_QSTR_layer_artdmx_infoObj,                                       // give it a name
    .print = addressable_layer_artdmx_info_print,                               // give it a print-function
    .make_new = addressable_layer_artdmx_info_make_new,                         // give it a constructor
    .locals_dict = (mp_obj_dict_t*)&addressable_layer_artdmx_info_locals_dict,  // and the global members
};


mp_obj_t addressable_layer_artdmx_info_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args ) {
    enum { ARG_leds, ARG_universe, ARG_start_index, ARG_start_channel, ARG_cpl, ARG_layer };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_leds,             MP_ARG_INT,                     {.u_int = 0} },
        { MP_QSTR_universe,         MP_ARG_INT,                     {.u_int = 0} },
        { MP_QSTR_start_index,      MP_ARG_INT,                     {.u_int = 0} },
        { MP_QSTR_start_channel,    MP_ARG_INT,                     {.u_int = 0} },
        { MP_QSTR_cpl,              MP_ARG_INT,                     {.u_int = 3} }, // pass in 4 if you want alpha to be supplied by the artdmx stream as well
        { MP_QSTR_layer,            MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    
    // // this checks the number of arguments (min 1, max 1);
    // // on error -> raise python exception
    // mp_arg_check_num(n_args, n_kw, 1, 1, true);

    // Allocate using the system allocator because we don't want micropython to be in charge of this allocation
    // create a new object of our C-struct type
    addressable_layer_artdmx_info_obj_t *self = (addressable_layer_artdmx_info_obj_t*)ARTNET_MALLOC(1*sizeof(addressable_layer_artdmx_info_obj_t));
    if( self == NULL ){
        mp_raise_OSError(MP_ENOMEM);
        return mp_const_none; 
    }
    memset((void*)self, 0x00, sizeof(addressable_layer_artdmx_info_obj_t));
    self->base.type = &addressable_layer_artdmx_infoObj_type; // give it a type
    self->leds = args[ARG_leds].u_int;
    self->portAddress = args[ARG_universe].u_int;
    self->start_index = args[ARG_start_index].u_int;
    self->start_channel = args[ARG_start_channel].u_int;
    self->cpl = args[ARG_cpl].u_int;
    self->layer = args[ARG_layer].u_obj;

    // printf("created dmx info object with: leds = %d, portAddress = %d, start_index = %d, start_channel = %d, layer = 0x%08X\n",self->leds, self->portAddress, self->start_index, self->start_channel, (uint32_t)self->layer);
    
    return MP_OBJ_FROM_PTR(self);
}

STATIC void addressable_layer_artdmx_info_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind ) {
    // get a ptr to the C-struct of the object
    addressable_layer_artdmx_info_obj_t *self = MP_OBJ_TO_PTR(self_in);
    printf ("Layer ArtDMX Info class object for universe %d\n", self->portAddress );
}




addressable_layer_artdmx_info_node_iter_t addressable_layer_artdmx_info_first( addressable_layer_artdmx_info_node_iter_t head ){ return head; }
bool addressable_layer_artdmx_info_done( addressable_layer_artdmx_info_node_iter_t iter ){ return (iter == NULL); }
addressable_layer_artdmx_info_node_iter_t addressable_layer_artdmx_info_next( addressable_layer_artdmx_info_node_iter_t iter ){ return (ADDRESSABLE_LAYER_ARTDMX_INFO_PTR_FROM_ITER(iter)->next); }
void addressable_layer_artdmx_info_foreach(addressable_layer_artdmx_info_node_iter_t head, void (*f)(addressable_layer_artdmx_info_node_iter_t iter, void*), void* args){
    if( head == NULL ){ return; }
    if(f == NULL){ return; }
    addressable_layer_artdmx_info_node_iter_t iter = NULL;
    for( iter = addressable_layer_artdmx_info_first(head); !addressable_layer_artdmx_info_done(iter); iter = addressable_layer_artdmx_info_next(iter) ){ 
        f(iter, args); 
    }
}

addressable_layer_artdmx_info_node_t* addressable_layer_artdmx_new_info_node( void ){
    addressable_layer_artdmx_info_node_t* node = NULL;
    node = (addressable_layer_artdmx_info_node_t*)ARTNET_MALLOC(1*sizeof(addressable_layer_artdmx_info_node_t));
    if(node == NULL){ return node; }
    memset((void*)node, 0x00, sizeof(addressable_layer_artdmx_info_node_t));
    return node;
}

addressable_layer_artdmx_info_node_t* addressable_layer_artdmx_info_node_predecessor( addressable_layer_artdmx_info_node_iter_t base, addressable_layer_artdmx_info_node_t* successor ){
    addressable_layer_artdmx_info_node_iter_t iter = NULL;
    if( base == NULL ){ return NULL; }
    for( iter = addressable_layer_artdmx_info_first(ADDRESSABLE_LAYER_ITER_FROM_ARTDMX_INFO_PTR( base )); !addressable_layer_artdmx_info_done(iter); iter = addressable_layer_artdmx_info_next(iter) ){
        if(ADDRESSABLE_LAYER_ARTDMX_INFO_PTR_FROM_ITER(iter)->next == successor){ break; }
    }
    return ADDRESSABLE_LAYER_ARTDMX_INFO_PTR_FROM_ITER(iter);
}

addressable_layer_artdmx_info_node_t* addressable_layer_artdmx_info_node_tail( addressable_layer_artdmx_info_node_iter_t base ){
    return addressable_layer_artdmx_info_node_predecessor(base, NULL);
}

int8_t addressable_layer_artdmx_info_node_append( addressable_layer_artdmx_info_node_iter_t base, addressable_layer_artdmx_info_node_t* node ){
    if( node == NULL ){ return -1; }
    node->next = NULL;
    addressable_layer_artdmx_info_node_t* tail = addressable_layer_artdmx_info_node_tail(base);
    if(tail == NULL){ return -1; }
    tail->next = node;
    return 0;
}

// addressable_layer_artdmx_info_node_t* addressable_layer_artdmx_info_append( addressable_layer_artdmx_info_node_iter_t base, addressable_layer_artdmx_info_obj_t* info ){
//     addressable_layer_artdmx_info_node_t* node = addressable_layer_artdmx_new_info_node();
//     if( node == NULL ){ return node; }
//     node->artdmx_info = info;
//     node->next = NULL;
//     addressable_layer_artdmx_info_node_t* tail = addressable_layer_artdmx_info_node_tail(base);
//     tail->next = node;
//     return node;
// }











// static void initialise_wifi(void);
// static esp_err_t event_handler(void *ctx, system_event_t *event);

// /* FreeRTOS event group to signal when we are connected & ready to make a request */
// static EventGroupHandle_t wifi_event_group;

// const int IPV4_GOTIP_BIT = BIT0;
// const int IPV6_GOTIP_BIT = BIT1;


// static esp_err_t event_handler(void *ctx, system_event_t *event)
// {
//     switch (event->event_id) {
//     case SYSTEM_EVENT_STA_START:
//         esp_wifi_connect();
//         ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
//         break;
//     case SYSTEM_EVENT_STA_CONNECTED:
//         /* enable ipv6 */
//         tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
//         break;
//     case SYSTEM_EVENT_STA_GOT_IP:
//         xEventGroupSetBits(wifi_event_group, IPV4_GOTIP_BIT);
//         ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
//         break;
//     case SYSTEM_EVENT_STA_DISCONNECTED:
//         /* This is a workaround as ESP32 WiFi libs don't currently auto-reassociate. */
//         esp_wifi_connect();
//         xEventGroupClearBits(wifi_event_group, IPV4_GOTIP_BIT);
//         xEventGroupClearBits(wifi_event_group, IPV6_GOTIP_BIT);
//         break;
//     case SYSTEM_EVENT_AP_STA_GOT_IP6:
//         xEventGroupSetBits(wifi_event_group, IPV6_GOTIP_BIT);
//         ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP6");

//         char *ip6 = ip6addr_ntoa(&event->event_info.got_ip6.ip6_info.ip);
//         ESP_LOGI(TAG, "IPv6: %s", ip6);
//     default:
//         break;
//     }
//     return ESP_OK;
// }