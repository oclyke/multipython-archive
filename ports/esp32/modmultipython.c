/*

The multipython module is designed to allow creation and execution of new MicroPython interpreters

*/

#include "py/binary.h"
#include "py/builtin.h"
#include "py/compile.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "py/mpstate.h"
#include "py/nlr.h"
#include "py/obj.h"
#include "py/parse.h"
#include "py/runtime.h"
#include "py/stackctrl.h"

#include "lib/mp-readline/readline.h"
#include "lib/utils/pyexec.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task.h"

#include "esp_task.h"
#include "soc/cpu.h"

#include "modmachine.h"

#include <string.h>

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


// MicroPython runs as a task under FreeRTOS
#define MULTIPYTHON_TASK_PRIORITY        (ESP_TASK_PRIO_MIN + 1)
#define MULTIPYTHON_TASK_STACK_SIZE      (16 * 1024)
#define MULTIPYTHON_TASK_STACK_LEN       (MULTIPYTHON_TASK_STACK_SIZE / sizeof(StackType_t))

mp_obj_t execute_from_str(const char *str) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr src_name = 1/*MP_QSTR_*/;
        mp_lexer_t *lex = mp_lexer_new_from_str_len(src_name, str, strlen(str), false);
        mp_parse_tree_t pt = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t module_fun = mp_compile(&pt, src_name, MP_EMIT_OPT_NONE, false);
        mp_call_function_0(module_fun);
        nlr_pop();
        return 0;
    } else {
        // uncaught exception
        return (mp_obj_t)nlr.ret_val;
    }
}

void testTask( void* void_context ){
    // when a new task spawns it already has a context allocated, but
    // it is up to the task to set the context id correctly
    mp_context_node_t* context = (mp_context_node_t*)void_context;
    context->id = mp_current_tID;
    mp_context_switch(context);
    
    volatile uint32_t sp = (uint32_t)get_sp();
    #if MICROPY_PY_THREAD
    mp_thread_init(pxTaskGetStackStart(NULL), MULTIPYTHON_TASK_STACK_LEN);
    #endif
    // uart_init();

    #if CONFIG_SPIRAM_SUPPORT
    // Try to use the entire external SPIRAM directly for the heap
    size_t mp_task_heap_size;
    void *mp_task_heap = (void*)0x3f800000;
    switch (esp_spiram_get_chip_size()) {
        case ESP_SPIRAM_SIZE_16MBITS:
            mp_task_heap_size = 2 * 1024 * 1024;
            break;
        case ESP_SPIRAM_SIZE_32MBITS:
        case ESP_SPIRAM_SIZE_64MBITS:
            mp_task_heap_size = 4 * 1024 * 1024;
            break;
        default:
            // // No SPIRAM, fallback to normal allocation
            // mp_task_heap_size = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
            mp_task_heap_size = 37000; // todo: temporary trying to reduce heap usage
            // size_t mp_task_heap_size = 10000; // todo: temporary trying to reduce heap usage
            mp_task_heap = mp_task_alloc( mp_task_heap_size, mp_current_tID ); // todo: allow the task to decide on the GC heap size (user input or something)
            break;
    }
    #else
    // // Allocate the uPy heap using mp_task_alloc and get the largest available region
    // size_t mp_task_heap_size = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    size_t mp_task_heap_size = 37000; // todo: temporary trying to reduce heap usage
    // size_t mp_task_heap_size = 10000; // todo: temporary trying to reduce heap usage
    void *mp_task_heap = mp_task_alloc( mp_task_heap_size, mp_current_tID ); // todo: allow the task to decide on the GC heap size (user input or something)
    #endif

soft_reset:
    // initialise the stack pointer for the main thread
    mp_stack_set_top((void *)sp);
    mp_stack_set_limit(MULTIPYTHON_TASK_STACK_SIZE - 1024);
    gc_init(mp_task_heap, mp_task_heap + mp_task_heap_size);
    mp_init();
    mp_obj_list_init(mp_sys_path, 0);
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_));
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_lib));
    mp_obj_list_init(mp_sys_argv, 0);
    mp_context_refresh();
    readline_init0();

    // // initialise peripherals
    // machine_pins_init();

    // // run boot-up scripts
    // pyexec_frozen_module("_boot.py");
    // pyexec_file("boot.py");
    // if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL) {
    //     pyexec_file("main.py");
    // }

    uint8_t reset = 0;

    // for (;;) {
        // if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
        //     vprintf_like_t vprintf_log = esp_log_set_vprintf(vprintf_null);
        //     if (pyexec_raw_repl() != 0) {
        //         break;
        //     }
        //     esp_log_set_vprintf(vprintf_log);
        // } else {
        //     if (pyexec_friendly_repl() != 0) {
        //         break;
        //     }
        // }

        // const char str0[] = "print('this is str0, baby!')";
        // const char str1[] = "print('hasta la vista, baby! - str1')";
        // const char str2[] = "print('never gonna give you up, never gonna let you down - str2')";
        // const char str3[] = "print('get over here - str3')";

        // const char* strns[4] = {str0, str1, str2, str3};

        // uint8_t which = 0;
        // // uint8_t which = ((uint32_t)pvParams);
        // if( which > 3){ which = 3; }

        if( context->args.source != NULL ){
            if ( execute_from_str( (char*)(context->args.source) ) ){
                printf("Error. Current context ID: 0x%x\n", (uint32_t)context->id);
            }
            // if (execute_from_str(strns[which])) {
            //     printf("Error. Current context ID: 0x%X\n", (uint32_t)context->id);
            // }
            else{
                // printf("Current state pointer: 0x%x\n", (uint32_t)p_mp_active_state_ctx);
            }
        }
        // vTaskDelay(2000/portTICK_PERIOD_MS);

        // int pyexec_raw_repl(void);
        // int pyexec_friendly_repl(void);
        // int pyexec_file(const char *filename);
        // int pyexec_frozen_module(const char *name);

        // if(args->input_kind == MP_PARSE_FILE_INPUT){
        //     // Interpret the source as a string filename
        //     if(pyexec_file((char*)(args->source)) == 0){
        //         reset = 0; // if no errors then exit was intentional
        //     }
        // }else if(args->input_kind == MP_PARSE_SINGLE_INPUT){
        //     // Interpret the source as a string input
        // }else{
        //     // Don't do anything... 
        // }

        // break; // used to leave this loop!
    // }

    // machine_timer_deinit_all();

    // #if MICROPY_PY_THREAD
    // mp_thread_deinit();
    // #endif

    gc_sweep_all();

    // // deinitialise peripherals
    // machine_pins_deinit();
    // usocket_events_deinit();

    mp_deinit();
    fflush(stdout);
    // const uint8_t reset = 0;
    if(reset){ // todo: eventually we will want to be able to catch errors and restart, but allow the task to go to end if it ended of it's own accord
        vTaskDelay(500/portTICK_PERIOD_MS);
        mp_hal_stdout_tx_str("MPY task: soft restart\r\n"); // todo: add more info saying which task is restarting
        goto soft_reset;
    }

    // mp_hal_stdout_tx_str("MPY task: death (debug output)\r\n");

    portENTER_CRITICAL(&mux);
    mp_task_remove( mp_current_tID );
    portEXIT_CRITICAL(&mux);
    vTaskDelete(NULL);  // When a task deletes itself make sure to release the mux *before* dying
}

STATIC mp_obj_t testCompilationUsingxTaskCreate( void ){
    static uint32_t temp_modmultipython_num_tasks = 0;
    xTaskCreate(testTask, "test_task", MULTIPYTHON_TASK_STACK_LEN, (void*)temp_modmultipython_num_tasks++, MULTIPYTHON_TASK_PRIORITY+1, NULL);
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
    if(( task == NULL ) || ( context == NULL ) || ( context == mp_context_head )){
        mp_print_str(&mp_plat_print, "Error. You must present a valid task to kill\n");
        return mp_const_none;
    }

    portENTER_CRITICAL(&mux);
    mp_task_remove( taskID );
    vTaskDelete(task);     
    portEXIT_CRITICAL(&mux);     
    return mp_const_none; // todo: make sure IDLE0 task is not starved so that the task's resources can actually be freed
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(multipython_task_end_obj, multipython_task_end);

STATIC mp_obj_t multipython_multiarg(size_t n_args, const mp_obj_t *args) {

    if(n_args == 0){ return mp_const_none; }

    mp_context_node_t* context = mp_task_register( 0, NULL ); // register a task with unknown ID and NULL additional arguments
    if( context == NULL ){ mp_print_str(&mp_plat_print, "Error. No memory for new context\n"); return mp_const_none; }

    const char *str = mp_obj_str_get_str(args[0]);
    size_t len = strlen(str) + 1;

    void* source = mp_context_dynmem_alloc( len, context ); // allocate global memory tied to the allocated context
    if( source == NULL ){ 
        mp_context_remove( context );
        mp_print_str(&mp_plat_print, "Error. No memory for new source\n"); 
        return mp_const_none; 
    }
    memcpy(source, (void*)str, len);
    context->args.input_kind = MP_PARSE_FILE_INPUT;
    context->args.source = source;

    xTaskCreate(testTask, "", MULTIPYTHON_TASK_STACK_LEN, (void*)context, MULTIPYTHON_TASK_PRIORITY+1, NULL);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(multipython_multiarg_obj, 1, 1, multipython_multiarg);

STATIC mp_obj_t multipython_tasks_clean_all( void ) {
    mp_context_iter_t iter = NULL;
    mp_print_str(&mp_plat_print, "Removing all MultiPython contexts\n");
    for( iter = mp_context_iter_first(mp_context_iter_next(MP_ITER_FROM_CONTEXT_PTR(mp_context_head))); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter) ){
        // Method four..... applying what I just learned.
        mp_context_node_t* context = MP_CONTEXT_PTR_FROM_ITER(iter);
        xTaskHandle task = (xTaskHandle)(context->id);  // need this b/c cant get it from context after removing context
        if(( task == NULL ) || ( context == NULL ) || ( context == mp_context_head )){
            mp_print_str(&mp_plat_print, "Error. You must present a valid task to kill\n");
            return mp_const_none;
        }
        portENTER_CRITICAL(&mux);
        mp_context_remove( context );
        vTaskDelete( task ); // OH! Duh... once we remove 'context' we can't use 'context' to get the task ID to remove.
        portEXIT_CRITICAL(&mux);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_tasks_clean_all_obj, multipython_tasks_clean_all);



// Module Definitions

STATIC const mp_rom_map_elem_t mp_module_multipython_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_multipython) },
    { MP_ROM_QSTR(MP_QSTR_show_context_list), MP_ROM_PTR(&multipython_show_context_list_obj) },
    { MP_ROM_QSTR(MP_QSTR_show_context_dynmem), MP_ROM_PTR(&multipython_show_context_dynmem_obj) },
    { MP_ROM_QSTR(MP_QSTR_new_context), MP_ROM_PTR(&multipython_new_context_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove_task), MP_ROM_PTR(&multipython_remove_task_obj) },
    // { MP_ROM_QSTR(MP_QSTR_new_task), MP_ROM_PTR(&multipython_new_task_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_task_id), MP_ROM_PTR(&getTaskID_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_context_by_id), MP_ROM_PTR(&multipython_get_context_by_id_obj) },
    { MP_ROM_QSTR(MP_QSTR_task_allocate), MP_ROM_PTR(&multipython_task_allocate_obj) },
    { MP_ROM_QSTR(MP_QSTR_task_free), MP_ROM_PTR(&multipython_task_free_obj) },
    { MP_ROM_QSTR(MP_QSTR_task_free_all), MP_ROM_PTR(&multipython_task_free_all_obj) },
    { MP_ROM_QSTR(MP_QSTR_task_end), MP_ROM_PTR(&multipython_task_end_obj) },
    { MP_ROM_QSTR(MP_QSTR_new_task_source), MP_ROM_PTR(&multipython_multiarg_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop_all_tasks), MP_ROM_PTR(&multipython_tasks_clean_all_obj) },
    
};
STATIC MP_DEFINE_CONST_DICT(mp_module_multipython_globals, mp_module_multipython_globals_table);

const mp_obj_module_t mp_module_multipython = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_multipython_globals,
};