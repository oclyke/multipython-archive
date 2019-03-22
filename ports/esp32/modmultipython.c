/*

The multipython module is designed to allow creation and execution of new MicroPython interpreters

*/

#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"
#include "py/builtin.h"

#include "py/mpstate.h"

// extern mp_context_node_t* mp_context_head;
// extern mp_context_node_t mp_active_context;

// void mp_context_refresh( void );
// void mp_context_switch(mp_context_node_t* node);

// void mp_task_register( uint32_t tID );
// void mp_task_switched_in( uint32_t tID );

// typedef mp_context_node_t* mp_context_iter_t;
// mp_context_iter_t mp_context_iter_first( mp_context_iter_t head );
// bool mp_context_iter_done( mp_context_iter_t iter );
// mp_context_iter_t mp_context_iter_next( mp_context_iter_t iter );
// void mp_context_foreach(mp_context_iter_t head, void (*f)(mp_context_iter_t iter, void*), void* args);



// Temporary testing functions
STATIC mp_obj_t multipython_show_context_list( void ) {
    mp_context_iter_t iter = NULL;
    uint32_t count = 0;
    mp_print_str(&mp_plat_print, "Context Info:\n");
    for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
        mp_printf(&mp_plat_print, "%08X: ID 0x%8X, state 0x%08X\n", count++, (MP_CONTEXT_PTR_FROM_ITER(iter)->id), (MP_CONTEXT_PTR_FROM_ITER(iter)->state) );
    }
    mp_print_str(&mp_plat_print, "--- END ---\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_show_context_list_obj, multipython_show_context_list);

STATIC mp_obj_t multipython_new_context( void ) {
    static uint32_t newcontextcount = 0;
    mp_task_register( ++newcontextcount );
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_new_context_obj, multipython_new_context);

STATIC mp_obj_t multipython_remove_task( mp_obj_t tID ) {
    mp_task_remove( mp_obj_int_get_truncated(tID) );
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(multipython_remove_task_obj, multipython_remove_task);










// Module Definitions

STATIC const mp_rom_map_elem_t mp_module_multipython_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_multipython) },
    { MP_ROM_QSTR(MP_QSTR_show_context_list), MP_ROM_PTR(&multipython_show_context_list_obj) },
    { MP_ROM_QSTR(MP_QSTR_new_context), MP_ROM_PTR(&multipython_new_context_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove_task), MP_ROM_PTR(&multipython_remove_task_obj) },

};
STATIC MP_DEFINE_CONST_DICT(mp_module_multipython_globals, mp_module_multipython_globals_table);

const mp_obj_module_t mp_module_multipython = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_multipython_globals,
};