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

#include "modaddressable_output.h"



modadd_status_e modadd_output_initialize( modadd_ctrl_t* ctrl ){
    if( ctrl == NULL ){ return MODADD_STAT_ERR; }
    if( ctrl->output.is_initialized ){ return MODADD_STAT_ERR; }
    if( ctrl->output.init == NULL ){ return MODADD_STAT_ERR; }
    modadd_status_e ret = ctrl->output.init( ctrl ); // call the proper init function for this output
    return ret;
}

modadd_status_e modadd_output_compose( modadd_ctrl_t* ctrl ){ // 
    if( ctrl == NULL ){ return MODADD_STAT_ERR; }
    if( ctrl->output.is_initialized ){ return MODADD_STAT_ERR; }
    if( ctrl->output.init == NULL ){ return MODADD_STAT_ERR; }
    modadd_status_e ret = ctrl->output.init( ctrl ); // call the proper init function for this output
    return ret;
}


modadd_status_e mach1_output_init_apa102_hw( modadd_ctrl_t* ctrl ){
    if( ctrl == NULL ){ return MODADD_STAT_ERR; }
    modadd_output_t* output = &(ctrl->output);
    // printf("starting to initialize hardware apa102 port\n");
    if( output == NULL ){ return MODADD_STAT_ERR; }
    // printf("output is good\n");
    if( output->protocol != MODADD_PROTOCOL_APA102 ){ return MODADD_STAT_ERR; }
    // printf("protocol is good \n");
    if( output->is_initialized ){ return MODADD_STAT_ERR; }
    // printf("port can be initialized!");

    modadd_port_spi_t* port = (modadd_port_spi_t*)output->port;
    esp_err_t ret;

    //Configuration for the SPI bus
    spi_bus_config_t buscfg;
    memset((void*)&buscfg, 0x00, sizeof(spi_bus_config_t));
    buscfg.mosi_io_num = port->dat;
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = port->clk;
    buscfg.quadwp_io_num =- 1;
    buscfg.quadhd_io_num = -1;
    // buscfg.max_transfer_sz = 150*4*8; // todo: don't hardcode this

    spi_device_interface_config_t devcfg;
    memset((void*)&devcfg, 0x00, sizeof(spi_device_interface_config_t));
    devcfg.clock_speed_hz = port->freq;           
    devcfg.spics_io_num = -1;                       //CS pin
    devcfg.queue_size = 1;                          //We want to be able to queue 7 transactions at a time
    devcfg.command_bits = 0;
    devcfg.address_bits = 0;
    devcfg.dummy_bits = 0;
    devcfg.mode = 3;                                //SPI mode 3
    // devcfg.pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    // devcfg.post_cb=post_dma_callback,

    ret=spi_bus_initialize(VSPI_HOST, &buscfg, port->dma_chan);
    ESP_ERROR_CHECK(ret);

    ret=spi_bus_add_device(VSPI_HOST, &devcfg, &(port->handle));
    ESP_ERROR_CHECK(ret);

    (void)ret;

    // printf("done initializing HW APA102 port\n");

    output->is_initialized = true; // block additional configuration attempts
    return MODADD_STAT_OK;
}

modadd_status_e mach1_output_init_apa102_sw( modadd_ctrl_t* ctrl ){
    if( ctrl == NULL ){ return MODADD_STAT_ERR; }
    modadd_output_t* output = &(ctrl->output);
    if( output == NULL ){ return MODADD_STAT_ERR; }
    if( output->protocol != MODADD_PROTOCOL_APA102 ){ return MODADD_STAT_ERR; }
    if( output->is_initialized ){ return MODADD_STAT_ERR; }

    modadd_port_sw_t* port = (modadd_port_sw_t*)output->port;

    gpio_set_direction(port->clk, GPIO_MODE_OUTPUT);
    gpio_set_direction(port->dat, GPIO_MODE_OUTPUT);

    printf("initializing SW apa102! clk = %d\n", port->clk);

    output->is_initialized = true; // block additional configuration attempts
    return MODADD_STAT_OK;
}












IRAM_ATTR void mach1_output_apa102_hw(void* arg){
    esp_err_t ret;
    modadd_ctrl_t* ctrl = (modadd_ctrl_t*)arg;                              // cast argument to control structure pointer
    modadd_port_spi_t* spi_port = (modadd_port_spi_t*)ctrl->output.port;

    memset(&(spi_port->transfer), 0, sizeof(spi_transaction_t));
    spi_port->transfer.length = 8*(ctrl->fixture_ctrl.data_len);
    spi_port->transfer.tx_buffer = (void*)ctrl->fixture_ctrl.data;

    // queue a transaction
    ret=spi_device_queue_trans(spi_port->handle, &(spi_port->transfer), portMAX_DELAY);
    if( ret != ESP_OK ){ printf("LED output DMA queue failed - try recomputing string memory!\n"); }
    // printf("transmission queue returned: %s\n", esp_err_to_name(ret));
    // assert(ret==ESP_OK);
    // // printf("queued a transaction\n");
}

IRAM_ATTR void mach1_output_apa102_sw(void* arg){
    // simply shift out the stat led data
    // the stat led data is statically allocated with a fixed length
    if( arg == NULL ){ return; }
    modadd_ctrl_t* ctrl = (modadd_ctrl_t*)arg; // 
    modadd_port_sw_t* port = (modadd_port_sw_t*)ctrl->output.port;
    modadd_fixture_ctrl_t fixture_ctrl = ctrl->fixture_ctrl;

    for( uint8_t indi = 0; indi < fixture_ctrl.data_len; indi++ ){
        uint8_t bite = fixture_ctrl.data[indi];
        for(uint8_t pos = 0; pos < 8; pos++){
            if( bite & (0x80 >> pos) ){ gpio_set_level(port->dat, 1); }
            else{ gpio_set_level(port->dat, 0); }
            gpio_set_level(port->clk, 0);
            gpio_set_level(port->clk, 1);
        }
    }
}