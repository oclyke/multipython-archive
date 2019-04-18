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

#ifndef _MODADDRESSABLE_TYPES_H_
#define _MODADDRESSABLE_TYPES_H_

#include <stdint.h>
#include <stdbool.h>

#include "esp_timer.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

typedef enum {
    MODADD_STAT_OK = 0x00,
    MODADD_STAT_ERR,
}modadd_status_e;

typedef enum {
    MODADD_PROTOCOL_APA102 = 0x00,
    MODADD_PROTOCOL_WS2812,

    MODADD_PROTOCOLS_NUM,
    MODADD_PROTOCOL_UNKNOWN,
}modadd_protocols_e;

typedef enum {
    MACH1_CONTROLLER_STAT = 0x00,
    MACH1_CONTROLLER_ALED,

    MACH1_CONTROLLERS_NUM,
    MACH1_CONTROLLER_UNKNOWN,
}modadd_controllers_e;

typedef struct _modadd_ctrl_t modadd_ctrl_t;



///////////////////////////////////////////////////////////////////////////
/* Fixture Types                                                         */
///////////////////////////////////////////////////////////////////////////

#define MODADD_FIXTURE_PTR_FROM_ITER(iter) ((modadd_fixture_t*)iter)
#define MODADD_ITER_FROM_FIXTURE_PTR(fptr) ((modadd_fixture_iter_t)fptr)

typedef uint16_t modadd_fixt_id_t;
typedef uint32_t modadd_fixt_offset_t;
typedef float modadd_trans_t;
typedef float modadd_rot_t;

typedef struct _modadd_fixture_trans_t{ // these should point to arrays with length >= corresponding fixture.leds
    modadd_trans_t*     x;              // todo: reconsider storing rotation / translation data on the ESP32... maybe OK just to use it on the phone?
    modadd_trans_t*     y;
    modadd_trans_t*     z;
}modadd_fixture_trans_t;

typedef struct _modadd_fixture_rot_t{   // these should point to arrays with length >= corresponding fixture.leds
    modadd_trans_t*     r_phi;          // todo: reconsider storing rotation / translation data on the ESP32... maybe OK just to use it on the phone?
    modadd_trans_t*     r_psi;      // these may need to be re-named to reflect the components of modified rodruigez parameters as in the textbook
    modadd_trans_t*     r_theta;
}modadd_fixture_rot_t;

typedef enum{
    MODADD_R_INDEX = 0x00,
    MODADD_G_INDEX,
    MODADD_B_INDEX,
    MODADD_A_INDEX,
}modadd_color_ind_e;

typedef struct _modadd_protocol_t{
    const uint8_t               bpl;            // bytes per led
    const uint8_t*              or_mask;        // array of length bpl that will be OR'd with the data
    const modadd_color_ind_e*   indices;        // array of length bpl that shows which color goes where (good for R, G, B, and A)
    const uint8_t               num_leading;    // length of leading bytes (that are sent ahead of any fixture data)
    const uint8_t*              leading;        // the leading bytes to send, if any
    const uint8_t               num_trailing;   // length of trailing bytes 
    const uint8_t*              trailing;       // the trailing bytes to send, if any
}modadd_protocol_t;

typedef struct _modadd_fixture_ctrl_t modadd_fixture_ctrl_t;
typedef struct _modadd_fixture_t {
    char*                       name;
    modadd_fixt_id_t            id;
    modadd_protocols_e          protocol;
    uint32_t                    leds;
    modadd_ctrl_t*              ctrl;   // pointer to the control structure that this fixture is associated with
    // modadd_fixt_offset_t        offset; // starting led number in the whole chain of outputs. e.g. if the only fixture ahead of this fixture had 3 leds in it then the offset for this fixture is 3
    uint8_t*                    data;   // pointer to data for this fixture
    // modadd_fixture_trans_t*     trans;  // todo: reconsider storing rotation / translation data on the ESP32... maybe OK just to use it on the phone?
    // modadd_fixture_rot_t*       rot;
    struct _modadd_fixture_t*   next;
}modadd_fixture_t;
typedef modadd_fixture_t* modadd_fixture_iter_t;




///////////////////////////////////////////////////////////////////////////
/* Output Types                                                          */
///////////////////////////////////////////////////////////////////////////
typedef modadd_status_e (*modadd_fixture_append_t)(modadd_fixture_t* fixture, modadd_ctrl_t* ctrl);
typedef modadd_status_e (*modadd_fixture_remove_t)(modadd_fixture_t* fixture, modadd_ctrl_t* ctrl);
struct _modadd_fixture_ctrl_t{
    modadd_fixture_t*       head;
    uint8_t*                data;
    uint32_t                data_len;
    bool                    size_increased;
    modadd_fixture_append_t append;
    modadd_fixture_remove_t remove;
};

typedef enum {
    MODADD_TIMER_STAT_NONE = 0x00,
    MODADD_TIMER_STAT_RUNNING = (1 << 0),
}modadd_timer_status_mask_e;
typedef void (*timer_callback_t)(void* arg);
typedef struct _modadd_output_timer_t{
    uint64_t                    period;
    esp_timer_handle_t          timer_handle;
    modadd_timer_status_mask_e  status; // bitmask
    esp_timer_create_args_t     create_args;
    // timer_callback_t            callback; // now handled within 
    // void*                       callback_args; // callback args now standardized to the output structure pointer
}modadd_output_timer_t;

typedef struct _modadd_output_t modadd_output_t;
typedef modadd_status_e (*modadd_output_init_fn_t)( modadd_ctrl_t* ctrl );

typedef void (*spi_callback_t)(spi_transaction_t *trans);
typedef struct _modadd_port_spi_t{
    spi_device_handle_t     handle; // spi_port
    spi_transaction_t       transfer;
    uint8_t                 clk;
    uint8_t                 dat;
    uint8_t                 dma_chan;
    uint32_t                freq;
    spi_callback_t          post_xfer;
    spi_callback_t          pre_xfer;
    bool                    is_initialized;
}modadd_port_spi_t;

typedef struct _modadd_port_sw_t{
    uint8_t                 clk;
    uint8_t                 dat;
}modadd_port_sw_t;

struct _modadd_output_t{
    modadd_protocols_e      protocol;
    bool                    is_initialized;
    modadd_output_init_fn_t init;
    void*                   port;
};

struct _modadd_ctrl_t{
    const char*             name;
    modadd_output_timer_t   timer;
    modadd_output_t         output;
    modadd_fixture_ctrl_t   fixture_ctrl;
};




#endif // _MODADDRESSABLE_TYPES_H_