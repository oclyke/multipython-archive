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


#define ARTNET_ITER_FROM_CALLBACK_PTR(ptr) ((artnet_callback_node_iter_t)ptr)
#define ARTNET_CALLBACK_PTR_FROM_ITER(iter) ((artnet_callback_node_t*)iter)


volatile artnet_packet_t    artnet_packet; // todo: make sure this is in RAM?
artnet_callback_node_t*     artnet_callbacks_head;
xTaskHandle                 artnet_task_handle;
static int                  artnet_initialized = 0;
static const char*          TAG = "example";

typedef artnet_callback_node_t* artnet_callback_node_iter_t;



// forward declarations
STATIC mp_obj_t artnet_get_network_interface_active_fn( mp_obj_t interface );
STATIC mp_obj_t artnet_network_interface_active( mp_obj_t interface_active_fn );
STATIC mp_obj_t artnet_interface_activate( mp_obj_t interface_active_fn );
void artnet_call_callback_helper( artnet_callback_node_iter_t iter, void* args );
void artnet_callback_foreach(artnet_callback_node_iter_t head, void (*f)(artnet_callback_node_iter_t iter, void*), void* args);
static void artnet_server_task(void *pvParameters);


void artnet_debug_callback_helper( artnet_callback_node_iter_t iter, void* args );




int8_t artnet_add_callback_c_args( artnet_callback_args_t cb, void* args ){
    if( cb == NULL ){ return -1; }
    printf("Here??\n" ); 
    artnet_callback_node_t* node = artnet_append_callback_node();
    if( node == NULL ){ return -1; }
    printf("Adding callback 0x%08X\n", (uint32_t)cb ); 
    node->c_callback = cb;
    node->args = args;

// int artnet_remove_callback_node( artnet_callback_node_t* node );
    return 0;
}

int8_t artnet_add_callback_c( artnet_callback_t cb ){
    return artnet_add_callback_c_args( (artnet_callback_args_t)cb, NULL ); // todo: is this kind of thing legal??? calling conventions?
}



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

mp_obj_t artnet_add_callback( mp_obj_t callback ){
    if( !callback ){ return mp_const_none; }
    mp_printf(&mp_plat_print, "Here??\n" ); 
    artnet_callback_node_t* node = artnet_append_callback_node();
    if( node == NULL ){ return mp_const_none; }
    mp_printf(&mp_plat_print, "Adding callback 0x%08X\n", (uint32_t)node ); 
    node->p_callback = callback;
    node->p_context = mp_active_contexts[MICROPY_GET_CORE_INDEX]; // store the context from which this callback was registered

// int artnet_remove_callback_node( artnet_callback_node_t* node );
    return callback;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(artnet_add_callback_obj, artnet_add_callback);

mp_obj_t artnet_show_callbacks( void ){
    // artnet_callback_foreach( artnet_callbacks_head, artnet_call_callback_helper, NULL );
    artnet_callback_foreach( artnet_callbacks_head, artnet_debug_callback_helper, NULL );
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(artnet_show_callbacks_obj, artnet_show_callbacks);

mp_obj_t artnet_call_callbacks( void ){
    artnet_callback_foreach( artnet_callbacks_head, artnet_call_callback_helper, NULL );
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(artnet_call_callbacks_obj, artnet_call_callbacks);


// Module Definitions
STATIC const mp_rom_map_elem_t mp_module_artnet_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_artnet) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&artnet_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&artnet_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_add_callback), MP_ROM_PTR(&artnet_add_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_show_callbacks), MP_ROM_PTR(&artnet_show_callbacks_obj) },
    { MP_ROM_QSTR(MP_QSTR_call_callbacks), MP_ROM_PTR(&artnet_call_callbacks_obj) },
};
STATIC MP_DEFINE_CONST_DICT(mp_module_artnet_globals, mp_module_artnet_globals_table);

const mp_obj_module_t mp_module_artnet = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_artnet_globals,
};



#define EXAMPLE_WIFI_SSID "esp_net"// CONFIG_WIFI_SSID              // Now we just need to make this irrespective of the station, so that it can be an access point too
#define EXAMPLE_WIFI_PASS "artnet88"//CONFIG_WIFI_PASSWORD

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
            int len = recvfrom(sock, (void*)&artnet_packet, sizeof(artnet_packet), 0, (struct sockaddr *)&sourceAddr, &socklen);

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

                // Call callbacks!
                artnet_callback_foreach( artnet_callbacks_head, artnet_call_callback_helper, NULL );



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







artnet_callback_node_iter_t artnet_callback_iter_first( artnet_callback_node_iter_t head ){ return head; }
bool artnet_callback_iter_done( artnet_callback_node_iter_t iter ){ return (iter == NULL); }
artnet_callback_node_iter_t artnet_callback_iter_next( artnet_callback_node_iter_t iter ){ return (ARTNET_CALLBACK_PTR_FROM_ITER(iter)->next); }
void artnet_callback_foreach(artnet_callback_node_iter_t head, void (*f)(artnet_callback_node_iter_t iter, void*), void* args){
    if( head == NULL ){ return; }
    if(f == NULL){ return; }
    artnet_callback_node_iter_t iter = NULL;
    for( iter = artnet_callback_iter_first(head); !artnet_callback_iter_done(iter); iter = artnet_callback_iter_next(iter) ){ 
        f(iter, args); 
    }
}

artnet_callback_node_t* artnet_new_callback_node( void ){
    artnet_callback_node_t* node = NULL;
    node = (artnet_callback_node_t*)ARTNET_MALLOC(1*sizeof(artnet_callback_node_t));
    if(node == NULL){ return node; }
    memset((void*)node, 0x00, sizeof(artnet_callback_node_t));
    return node;
}

artnet_callback_node_t* artnet_callback_predecessor( artnet_callback_node_t* successor ){
    artnet_callback_node_iter_t iter = NULL;
    for( iter = artnet_callback_iter_first(ARTNET_ITER_FROM_CALLBACK_PTR(artnet_callbacks_head)); !artnet_callback_iter_done(iter); iter = artnet_callback_iter_next(iter) ){
        if(ARTNET_CALLBACK_PTR_FROM_ITER(iter)->next == successor){ break; }
    }
    return ARTNET_CALLBACK_PTR_FROM_ITER(iter);
}

artnet_callback_node_t* artnet_callback_tail( void ){
    return artnet_callback_predecessor(NULL);
}

artnet_callback_node_t* artnet_append_callback_node( void ){
    artnet_callback_node_t* node = artnet_new_callback_node();
    if( node == NULL ){ return node; }
    artnet_callback_node_t* tail = artnet_callback_tail();
    if( artnet_callbacks_head == NULL ){
        artnet_callbacks_head = node;
    }else{
        tail->next = node;
    }
    return node;
}

int artnet_remove_callback_node( artnet_callback_node_t* node ){
    if( node == NULL ){ return -1; }
    if( artnet_callbacks_head == node ){
        artnet_callbacks_head = node->next;
    }else{
        artnet_callback_node_t* predecessor = artnet_callback_predecessor( node );
        if( predecessor == NULL ){ return -1; }
        artnet_callback_node_t* successor = NULL;
        successor = node->next;
        predecessor->next = successor;
    }
    ARTNET_FREE( node );
    return 0;
}

void artnet_call_callback_helper( artnet_callback_node_iter_t iter, void* args ){
    // args will probably be NULL, all info is within the iterator
    artnet_callback_node_t* node = ARTNET_CALLBACK_PTR_FROM_ITER(iter);
    if( node == NULL ){ return; }
    if( node->c_callback ){
        // Call the c callback
        node->c_callback( node->args );
    }else if( node->p_callback ){ // for now we're skipping python callbacks
        // if( node->p_context ){
        // // if( mp_obj_is_type( node->p_callback, &mp_type_fun_builtin_0) ){
        //     printf("calling!!\n");
        //     mp_context_switch(node->p_context);
        //     // mp_obj_t res = mp_call_function_0(node->p_callback);
        //     mp_call_function_0(node->p_callback);
        // // }
        // }
    }

    // Note: todo: maybe necessary to switch into the MultiPython context from which the callback was added
    // mp_context_switch(node->p_context);
}

void artnet_debug_callback_helper( artnet_callback_node_iter_t iter, void* args ){

    printf("Callback node at 0x%08X\n", (uint32_t)iter);
    artnet_callback_node_t* node = ARTNET_CALLBACK_PTR_FROM_ITER(iter);
    if(node == NULL){
        printf("Node is null? what?\n");
        return; 
    }
    printf("c_callback: 0x%08X\n", (uint32_t)node->c_callback);
    printf("p_callback: 0x%08X\n", (uint32_t)node->p_callback);
    printf("p_context: 0x%08X\n", (uint32_t)node->p_context);
    if( node->p_context != NULL ){ printf("\tp_context->id: %d\n", (uint32_t)node->p_context->id); }
    printf("args: 0x%08X\n", (uint32_t)node->args);
    printf("next: 0x%08X\n", (uint32_t)node->next);
    printf("\n");

}










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