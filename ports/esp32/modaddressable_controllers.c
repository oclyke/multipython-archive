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
This file is used to store definitions of controllers that will be available on 
the device. 

This includes the controller structures and potentially callback functions as needed
*/

#include "modaddressable_controllers.h"


//////////////////////////////////////////////////////////////////////
/* Mach1 Status LED Controller Definitions                          */
//////////////////////////////////////////////////////////////////////

#define MACHONE_STAT_CLK 33
#define MACHONE_STAT_DAT 5
#define MACH1_STAT_OUTPUT_CALLBACK mach1_stat_output
#define MACH1_STAT_INIT_FN mach1_stat_init
#define MACHONE_STAT_TIMER_PERIOD 33333

#define MACHONE_STAT_DATA_BYTES 8
DRAM_ATTR uint8_t machone_stat_data[MACHONE_STAT_DATA_BYTES];

IRAM_ATTR static void mach1_stat_output(void* arg);
modadd_status_e mach1_stat_init( modadd_ctrl_t* ctrl );

modadd_port_sw_t machone_stat_sw_port = {
    .clk = MACHONE_STAT_CLK,
    .dat = MACHONE_STAT_DAT,
};

modadd_trans_t machone_stat_trans_x = 0;
modadd_trans_t machone_stat_trans_y = 0;
modadd_trans_t machone_stat_trans_z = 0;
modadd_fixture_trans_t machone_stat_trans = {
    .x = &machone_stat_trans_x,
    .y = &machone_stat_trans_y,
    .z = &machone_stat_trans_z,
};
modadd_rot_t machone_stat_rot_r_phi = 0;
modadd_rot_t machone_stat_rot_r_psi = 0;
modadd_rot_t machone_stat_rot_r_theta = 0;
modadd_fixture_rot_t machone_stat_rot = {
    .r_phi = &machone_stat_rot_r_phi,
    .r_psi = &machone_stat_rot_r_psi,
    .r_theta = &machone_stat_rot_r_theta,
};

addressable_fixture_obj_t machone_stat_fixture_obj = {
    .base = {
        .type = &addressable_fixtureObj_type,
    },
    .name = "MachOne Status LED",
    .id = 0x00,
    // .protocol = MODADD_PROTOCOL_APA102_SW,
    .leds = 1,
    .ctrl = NULL,
    .data = machone_stat_data + 4,
    .layers = NULL,
};

modadd_fixture_node_t machone_stat_fixture_node = {
    .fixture = &machone_stat_fixture_obj,
    .next = NULL,
};

modadd_ctrl_t mach1_stat_ctrl = {
    .name = "MachOne Addressable Ctrl Status",
    .timer = {
        .period = MACHONE_STAT_TIMER_PERIOD,
        // .timer,
        .status = MODADD_TIMER_STAT_NONE, 
        .create_args = {
            .callback = MACH1_STAT_OUTPUT_CALLBACK,   
        }
        // .callback = MACH1_STAT_OUTPUT_CALLBACK,
        // .callback_args = NULL,
    },
    .output = {
        .protocol = MODADD_PROTOCOL_APA102,
        .is_initialized = false,
        .init = MACH1_STAT_INIT_FN,
        .port = &machone_stat_sw_port,
    },
    .fixture_ctrl = {
        .head = &machone_stat_fixture_node,
        .data = machone_stat_data,
        .data_len = MACHONE_STAT_DATA_BYTES,
        .size_increased = false,
        .append = NULL, // this disallows appending fixtures to the stat LED
        .remove = NULL, // disallows removing fixtures from stat LED
    },
};


void mach1_stat_output(void* arg){  // arg should be a pointer to the stat output structure
    addressable_layer_compose(arg);
    mach1_output_apa102_sw( arg );
}

modadd_status_e mach1_stat_init( modadd_ctrl_t* ctrl ){
    return mach1_output_init_apa102_sw( ctrl );
}






//////////////////////////////////////////////////////////////////////
/* Mach1 Addressable LED Controller Definitions                     */
//////////////////////////////////////////////////////////////////////

#define MACH1_ALED_CLK (18)
#define MACH1_ALED_DAT (23)
#define MACH1_ALED_DMA_CHAN (2)
#define MACH1_ALED_OUTPUT_CALLBACK mach1_aled_output
#define MACH1_ALED_INIT_FN mach1_aled_init
#define MACH1_ALED_TIMER_PERIOD (33333)
// #define MACH1_ALED_FREQ (10*1000*1000)
#define MACH1_ALED_FREQ (5*1000*1000)

IRAM_ATTR static void mach1_aled_output(void* arg);
modadd_status_e mach1_aled_init( modadd_ctrl_t* ctrl );

modadd_port_spi_t mach1_aled_spi_port = {
    // .handle, // spi_port
    // .transfer,
    .clk = MACH1_ALED_CLK,
    .dat = MACH1_ALED_DAT,
    .freq = MACH1_ALED_FREQ,
    .post_xfer = NULL,
    .pre_xfer = NULL,
    .dma_chan = MACH1_ALED_DMA_CHAN,
};

modadd_ctrl_t mach1_aled_ctrl = {
    .name = "MachOne Addressable LED Ctrl0",
    .timer = {
        .period = MACH1_ALED_TIMER_PERIOD,
        // .timer,
        .status = MODADD_TIMER_STAT_NONE,
        .create_args = {
            .callback = MACH1_ALED_OUTPUT_CALLBACK,
            // .callback_args = NULL,
        }
    },
    .output = {
        .protocol = MODADD_PROTOCOL_UNKNOWN,
        .is_initialized = false,
        .init = MACH1_ALED_INIT_FN,
        .port = &mach1_aled_spi_port,
    },
    .fixture_ctrl = {
        .head = NULL,
        .data = NULL,
        .data_len = 0x00,
        .size_increased = false,
        .append = modadd_fixture_append,
        .remove = modadd_fixture_remove,
    },
};


void mach1_aled_output(void* arg){  // arg should be a pointer to the aled output structure
    addressable_layer_compose(arg);
    mach1_output_apa102_hw( arg );
}

modadd_status_e mach1_aled_init( modadd_ctrl_t* ctrl ){
    return mach1_output_init_apa102_hw( ctrl );
}




//////////////////////////////////////////////////////////////////////
/* Controllers Array Definition                                     */
//////////////////////////////////////////////////////////////////////
modadd_ctrl_t* modadd_controllers[MODADD_NUM_CONTROLLERS] = {
    &mach1_stat_ctrl,
    &mach1_aled_ctrl,
};