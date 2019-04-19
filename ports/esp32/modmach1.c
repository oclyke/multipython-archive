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
// #include "py/runtime.h"
// #include "py/stackctrl.h"

#include "esp_spiram.h"



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




STATIC const mp_rom_map_elem_t mp_module_mach1_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_mach1) },
    { MP_ROM_QSTR(MP_QSTR_system), MP_ROM_PTR(&mach1_system_obj) },
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
