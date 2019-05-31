#include "py/obj.h"
#include "py/mperrno.h"
#include "py/runtime.h"

#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#include <string.h>

#define SDMMC_MAX_STORAGE_DEVICES 1

// #define PIN_NUM_MISO 12
// #define PIN_NUM_MOSI 13
// #define PIN_NUM_CLK  14
// #define PIN_NUM_CS   26

#define SDMMC_DMA_CHAN 1

typedef struct _sdmmc_storage_device_t {
    sdmmc_card_t                        card;       
    sdmmc_host_t                        host;       // flags contain spi vs mmc info
    sdspi_slot_config_t                 sdslot;     // used for spi
    sdmmc_slot_config_t                 mmcslot;    // used for mmc
} sdmmc_storage_device_t;

const sdmmc_host_t sdspi_default_host_config = SDSPI_HOST_DEFAULT();
const sdmmc_host_t sdmmc_default_host_config = SDMMC_HOST_DEFAULT();
const sdspi_slot_config_t sdspi_default_slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
const sdmmc_slot_config_t sdmmc_default_slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

// sdmmc_storage_device_t sdmmc_devices[SDMMC_MAX_STORAGE_DEVICES];
sdmmc_storage_device_t sdmmc_dev;
bool sdmmc_dev_initialized = false;

esp_err_t sdmmc_init_device_sub( sdmmc_storage_device_t* dev ){
    esp_err_t err = ESP_OK;

    err = (*(dev->host.init))();
    if (err != ESP_OK) {
        mp_printf(&mp_plat_print, "ERROR: host init returned rc=0x%x\n", err);
        return err;
    }

    // configure SD slot
    if (dev->host.flags == SDMMC_HOST_FLAG_SPI) {
        err = sdspi_host_init_slot(dev->host.slot,
                (const sdspi_slot_config_t*) &(dev->sdslot));
    } else {
        err = sdmmc_host_init_slot(dev->host.slot,
                (const sdmmc_slot_config_t*) &(dev->mmcslot));
    }
    if (err != ESP_OK) {
        mp_printf(&mp_plat_print, "ERROR: slot_config returned rc=0x%x\n", err);
        return err;
    }

    // probe and initialize card
    err = sdmmc_card_init(&(dev->host), &(dev->card));
    if (err != ESP_OK) {
        mp_printf(&mp_plat_print, "ERROR: sdmmc_card_init failed 0x(%x)", err);
        return err;
    }

    return err;
}

esp_err_t sdmmc_init_device_host_slot_spi( sdmmc_storage_device_t* dev, const sdmmc_host_t* host_config, const sdspi_slot_config_t* spi_slot_config ){
    esp_err_t err = ESP_OK;
    dev->host = sdspi_default_host_config;
    dev->sdslot = sdspi_default_slot_config;
    memset( (void*)&(dev->mmcslot), 0x00, sizeof(sdmmc_slot_config_t) );
    memset( (void*)&(dev->card), 0x00, sizeof(sdmmc_card_t));
    if( host_config != NULL ){
        dev->host = *(host_config);
    }
    if( spi_slot_config != NULL ){
        dev->sdslot = *(spi_slot_config);
    }
    err = sdmmc_init_device_sub( dev );
    return err;
}

esp_err_t sdmmc_init_device_host_slot_mmc( sdmmc_storage_device_t* dev, const sdmmc_host_t* host_config, const sdmmc_slot_config_t* mmc_slot_config ){
    esp_err_t err = ESP_OK;
    dev->host = sdmmc_default_host_config;
    dev->mmcslot = sdmmc_default_slot_config;
    memset( (void*)&(dev->sdslot), 0x00, sizeof(sdspi_slot_config_t) );
    memset( (void*)&(dev->card), 0x00, sizeof(sdmmc_card_t));
    if( host_config != NULL ){
        dev->host = *(host_config);
    }
    if( mmc_slot_config != NULL ){
        dev->mmcslot = *(mmc_slot_config);
    }
    err = sdmmc_init_device_sub( dev );
    return err;
}

esp_err_t sdmmc_init_device( sdmmc_storage_device_t* dev, const sdmmc_host_t* host_config, const void* slot_config ){
    esp_err_t err = ESP_OK;
    if( host_config == NULL ){ return ESP_FAIL; }

    if( host_config->flags == SDMMC_HOST_FLAG_SPI ){
        err = sdmmc_init_device_host_slot_spi( dev, host_config, slot_config );
    }else{
        err = sdmmc_init_device_host_slot_mmc( dev, host_config, slot_config );
    }
    return err;
}

STATIC mp_obj_t sdmmc_init(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args){
    if(sdmmc_dev_initialized == true ){ return mp_const_true; }

    // set up args
    enum { ARG_use_mmc, ARG_miso, ARG_mosi, ARG_sclk, ARG_cs /*, ARG_dma_chan*/ };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_use_mmc,  MP_ARG_OBJ,                     {.u_obj = mp_const_true} },
        { MP_QSTR_miso,     MP_ARG_KW_ONLY | MP_ARG_INT,    {.u_int = -1} },
        { MP_QSTR_mosi,     MP_ARG_KW_ONLY | MP_ARG_INT,    {.u_int = -1} },
        { MP_QSTR_sclk,     MP_ARG_KW_ONLY | MP_ARG_INT,    {.u_int = -1} },
        { MP_QSTR_cs,       MP_ARG_KW_ONLY | MP_ARG_INT,    {.u_int = -1} },
        // { MP_QSTR_dma_chan, MP_ARG_KW_ONLY | MP_ARG_INT,    {.u_int = -1} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);


    esp_err_t err = ESP_OK;
    sdmmc_storage_device_t* dev = &sdmmc_dev;
    if( mp_obj_is_true( args[ARG_use_mmc].u_obj ) ){
        printf("Using SDMMC interface\n");
        // GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
        // Internal pull-ups are not sufficient. However, enabling internal pull-ups
        // does make a difference some boards, so we do that here.
        gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
        gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
        gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
        gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
        gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

        err = sdmmc_init_device( dev, &sdmmc_default_host_config, (void*)&sdmmc_default_slot_config );
    }else{
        printf("Using SPI interface");
        if( args[ARG_miso].u_int == -1 ){
            printf("You must provide the MISO pin number\n");
            return mp_const_none;
        }
        if( args[ARG_mosi].u_int == -1 ){
            printf("You must provide the MOSI pin number\n");
            return mp_const_none;
        }
        if( args[ARG_sclk].u_int == -1 ){
            printf("You must provide the SCLK pin number\n");
            return mp_const_none;
        }
        if( args[ARG_cs].u_int == -1 ){
            printf("You must provide the CS pin number\n");
            return mp_const_none;
        }

        sdspi_slot_config_t slot = SDSPI_SLOT_CONFIG_DEFAULT();
        slot.gpio_miso      = args[ARG_miso].u_int;
        slot.gpio_mosi      = args[ARG_mosi].u_int;
        slot.gpio_sck       = args[ARG_sclk].u_int;
        slot.gpio_cs        = args[ARG_cs].u_int;
        slot.dma_channel    = SDMMC_DMA_CHAN;

        err = sdmmc_init_device( dev, &sdspi_default_host_config, (void*)&slot );
    }
    if( err != ESP_OK ){
        // mp_raise_OSError(MP_ENODEV);
        return mp_const_none;
    }
    sdmmc_card_print_info(stdout, &dev->card);
    sdmmc_dev_initialized = true;
    return mp_const_true;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(sdmmc_init_obj, 0, sdmmc_init);

STATIC mp_obj_t sdmmc_read_blocks( mp_obj_t start_block, mp_obj_t num_blocks, mp_obj_t buf_in) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_WRITE);
    // if( bufinfo.len < (sdmmc_dev->card.) ) // check that buffer is long enough
    esp_err_t res = sdmmc_read_sectors(&sdmmc_dev.card, bufinfo.buf, mp_obj_get_int(start_block), mp_obj_get_int(num_blocks));
    if (res != ESP_OK) {
        mp_raise_OSError(MP_EIO);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(sdmmc_read_blocks_obj, sdmmc_read_blocks);

STATIC mp_obj_t sdmmc_write_blocks( mp_obj_t start_block, mp_obj_t num_blocks, mp_obj_t buf_in) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_WRITE);
    // if( bufinfo.len < (dev->card.) ) // check that buffer is long enough
    esp_err_t res = sdmmc_write_sectors(&sdmmc_dev.card, bufinfo.buf, mp_obj_get_int(start_block), mp_obj_get_int(num_blocks));
    if (res != ESP_OK) {
        mp_raise_OSError(MP_EIO);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(sdmmc_write_blocks_obj, sdmmc_write_blocks);

STATIC mp_obj_t sdmmc_get_num_blocks( void ) {
    return mp_obj_new_int(sdmmc_dev.card.csd.capacity);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(sdmmc_get_num_blocks_obj, sdmmc_get_num_blocks);

STATIC mp_obj_t sdmmc_get_block_size( void ) {
    return mp_obj_new_int(sdmmc_dev.card.csd.sector_size);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(sdmmc_get_block_size_obj, sdmmc_get_block_size);


// Module Definitions

STATIC const mp_rom_map_elem_t mp_module_sdmmc_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_sdmmc) },
    { MP_ROM_QSTR(MP_QSTR_write_blocks), MP_ROM_PTR(&sdmmc_write_blocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_blocks), MP_ROM_PTR(&sdmmc_read_blocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&sdmmc_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_num_blocks), MP_ROM_PTR(&sdmmc_get_num_blocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_block_size), MP_ROM_PTR(&sdmmc_get_block_size_obj) },
};
STATIC MP_DEFINE_CONST_DICT(mp_module_sdmmc_globals, mp_module_sdmmc_globals_table);

const mp_obj_module_t mp_module_sdmmc = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_sdmmc_globals,
};