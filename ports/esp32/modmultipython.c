/*

The multipython module is designed to allow creation and execution of new MicroPython interpreters

*/

#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"
#include "py/builtin.h"

#include "py/mpstate.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task.h"

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Temporary testing functions
STATIC mp_obj_t multipython_show_context_list( void ) {
    mp_context_iter_t iter = NULL;
    uint32_t count = 0;
    mp_print_str(&mp_plat_print, "Context Info:\n");
    for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
        mp_printf(&mp_plat_print, "%08X: ID 0x%8X, state 0x%08X, has dynmem %d\n", count++, (MP_CONTEXT_PTR_FROM_ITER(iter)->id), (MP_CONTEXT_PTR_FROM_ITER(iter)->state), (MP_CONTEXT_PTR_FROM_ITER(iter)->memhead != NULL) );
    }
    mp_print_str(&mp_plat_print, "--- END ---\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_show_context_list_obj, multipython_show_context_list);

STATIC mp_obj_t multipython_show_context_dynmem( void ) {
    mp_context_iter_t iter = NULL;
    uint32_t count = 0;
    mp_print_str(&mp_plat_print, "Context Info:\n");
    for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
        mp_printf(&mp_plat_print, "%08X: ID 0x%8X, state 0x%08X, has dynmem %d\n", count++, (MP_CONTEXT_PTR_FROM_ITER(iter)->id), (MP_CONTEXT_PTR_FROM_ITER(iter)->state), (MP_CONTEXT_PTR_FROM_ITER(iter)->memhead != NULL) );
        if((MP_CONTEXT_PTR_FROM_ITER(iter)->memhead != NULL)){
            mp_context_dynmem_iter_t memiter = NULL;
            for( memiter = mp_dynmem_iter_first(MP_ITER_FROM_DYNMEM_PTR(MP_CONTEXT_PTR_FROM_ITER(iter)->memhead)); !mp_dynmem_iter_done(memiter); memiter = mp_dynmem_iter_next(memiter) ){
                mp_printf(&mp_plat_print, "%+2sdynmem ctx 0x%8X,   mem 0x%8X, size 0x%08X\n", " ", (MP_CONTEXT_PTR_FROM_ITER(iter)->id), (MP_DYNMEM_PTR_FROM_ITER(memiter)->mem), (MP_DYNMEM_PTR_FROM_ITER(memiter)->size) );
            }
        }
    }
    mp_print_str(&mp_plat_print, "--- END ---\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_show_context_dynmem_obj, multipython_show_context_dynmem);

STATIC mp_obj_t multipython_new_context( void ) {
    static uint32_t newcontextcount = 0;
    mp_task_register( ++newcontextcount, NULL );
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_new_context_obj, multipython_new_context);

STATIC mp_obj_t multipython_remove_task( mp_obj_t tID ) {
    mp_task_remove( mp_obj_int_get_truncated(tID) );
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(multipython_remove_task_obj, multipython_remove_task);

// STATIC mp_obj_t multipython_allocate( mp_obj_t size ) {
//     void* mp_task_alloc( size_t size, uint32_t tID );

//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_1(multipython_allocate_obj, multipython_allocate);

// STATIC mp_obj_t multipython_free( ) {

//     int8_t mp_task_free( void* mem, uint32_t tID );
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_free_obj, multipython_free);



// MicroPython runs as a task under FreeRTOS
#define MULTIP_TASK_PRIORITY        (ESP_TASK_PRIO_MIN + 1)
#define MULTIP_TASK_STACK_SIZE      (16 * 1024)
#define MULTIP_TASK_STACK_LEN       (MULTIP_TASK_STACK_SIZE / sizeof(StackType_t))

void testTask( void* pvParams ){

    uint32_t thisTaskID = mp_current_tID;
    mp_task_register( thisTaskID, pvParams );

    uint8_t* p8 = (uint8_t*)mp_task_alloc( 1*sizeof(uint8_t), thisTaskID );
    if( p8 == NULL ){ 
        while(1){
            mp_print_str(&mp_plat_print, "Could not allocate p8\n"); 
            vTaskDelay(5000/portTICK_PERIOD_MS);
        }
    }

    *(p8) = 0xAA;

    for(;;){
        vTaskDelay(1000/portTICK_PERIOD_MS);
        mp_printf(&mp_plat_print, "Test Task: tID 0x%08X. p8: 0x%08X. *p8: %d\n", thisTaskID, (uint32_t)p8, *(p8) );
    }

    portENTER_CRITICAL(&mux);
    mp_task_remove( thisTaskID );
    portEXIT_CRITICAL(&mux);
    vTaskDelete(NULL);
}

STATIC mp_obj_t testCompilationUsingxTaskCreate( void ){

    xTaskCreate(testTask, "test_task", MULTIP_TASK_STACK_LEN, NULL, MULTIP_TASK_PRIORITY+1, NULL);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_new_task_obj, testCompilationUsingxTaskCreate);

STATIC mp_obj_t getTaskID( void ){
    mp_printf(&mp_plat_print, "The current TaskID is: 0x%8X\n", mp_current_tID );
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(getTaskID_obj, getTaskID);

extern mp_context_node_t* mp_context_by_tid( uint32_t tID );
STATIC mp_obj_t multipython_get_context_by_id( mp_obj_t tID ) {
    mp_context_node_t* context = mp_context_by_tid( mp_obj_int_get_truncated(tID) );
    mp_printf(&mp_plat_print, "The context for that ID is: 0x%8X\n", (uint32_t)context );
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(multipython_get_context_by_id_obj, multipython_get_context_by_id);

STATIC mp_obj_t multipython_task_allocate( mp_obj_t size ) {
    mp_task_alloc( (size_t)mp_obj_int_get_truncated(size), mp_current_tID );
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(multipython_task_allocate_obj, multipython_task_allocate);

STATIC mp_obj_t multipython_task_free( mp_obj_t ptr ) {
    int8_t retval = mp_task_free( (void*)mp_obj_int_get_truncated(ptr), mp_current_tID );
    if(retval){ mp_print_str(&mp_plat_print, "There was a problem freeing that memory\n"); }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(multipython_task_free_obj, multipython_task_free);

extern int8_t mp_task_free_all( uint32_t tID );
STATIC mp_obj_t multipython_task_free_all( void ) {
    int8_t retval= mp_task_free_all( mp_current_tID );
    if(retval){ mp_print_str(&mp_plat_print, "There was a problem freeing all memory\n"); }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_task_free_all_obj, multipython_task_free_all);

STATIC mp_obj_t multipython_task_end( mp_obj_t tID ) {
    // It so happens that in the ESP32 port (using FreeRTOS) the tID *is* the task handle, 
    // just as a uint32_t. So we can cast it to the xTaskHandle type 
    uint32_t taskID = mp_obj_int_get_truncated(tID);
    xTaskHandle task = (xTaskHandle)taskID;
    mp_context_node_t* context = mp_context_by_tid( taskID );
    if(( task == NULL ) || ( context == NULL )){
        mp_print_str(&mp_plat_print, "Error. You must present a valid task to kill\n");
        return mp_const_none;
    }
    portENTER_CRITICAL(&mux);
    mp_task_remove( taskID );
    portEXIT_CRITICAL(&mux);
    vTaskDelete(task);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(multipython_task_end_obj, multipython_task_end);




// Module Definitions

STATIC const mp_rom_map_elem_t mp_module_multipython_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_multipython) },
    { MP_ROM_QSTR(MP_QSTR_show_context_list), MP_ROM_PTR(&multipython_show_context_list_obj) },
    { MP_ROM_QSTR(MP_QSTR_show_context_dynmem), MP_ROM_PTR(&multipython_show_context_dynmem_obj) },
    { MP_ROM_QSTR(MP_QSTR_new_context), MP_ROM_PTR(&multipython_new_context_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove_task), MP_ROM_PTR(&multipython_remove_task_obj) },
    { MP_ROM_QSTR(MP_QSTR_new_task), MP_ROM_PTR(&multipython_new_task_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_task_id), MP_ROM_PTR(&getTaskID_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_context_by_id), MP_ROM_PTR(&multipython_get_context_by_id_obj) },
    { MP_ROM_QSTR(MP_QSTR_task_allocate), MP_ROM_PTR(&multipython_task_allocate_obj) },
    { MP_ROM_QSTR(MP_QSTR_task_free), MP_ROM_PTR(&multipython_task_free_obj) },
    { MP_ROM_QSTR(MP_QSTR_task_free_all), MP_ROM_PTR(&multipython_task_free_all_obj) },
    { MP_ROM_QSTR(MP_QSTR_task_end), MP_ROM_PTR(&multipython_task_end_obj) },
    
};
STATIC MP_DEFINE_CONST_DICT(mp_module_multipython_globals, mp_module_multipython_globals_table);

const mp_obj_module_t mp_module_multipython = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_multipython_globals,
};