/*

The multipython module is designed to allow creation and execution of new MicroPython interpreters

*/

#include "py/binary.h"
#include "py/builtin.h"
#include "py/compile.h"
#include "py/mperrno.h"
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

// MicroPython runs as a task under FreeRTOS
#define MULTIPYTHON_TASK_PRIORITY        (ESP_TASK_PRIO_MIN + 1)
#define MULTIPYTHON_TASK_STACK_SIZE      (16 * 1024)
#define MULTIPYTHON_TASK_STACK_LEN       (MULTIPYTHON_TASK_STACK_SIZE / sizeof(StackType_t))

// globals
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Forward declarations
mp_obj_t execute_from_str(const char *str);
void multipython_task_template( void* void_context );

// helper functions (not visible to users)
STATIC mp_obj_t multipython_get_context_dict( mp_context_node_t* context, mp_int_t position ) {
    if( context == NULL ){ return mp_const_none; }

    mp_obj_t context_dict = mp_obj_new_dict(7);

    const char* str_key_pos = "pos";
    const char* str_key_tID = "tID";
    const char* str_key_state = "state";
    const char* str_key_mem = "mem";
    const char* str_key_thread = "thread";
    const char* str_key_args = "args";
    const char* str_key_status = "status";

    mp_obj_t key_pos = mp_obj_new_str_via_qstr(str_key_pos, strlen(str_key_pos));
    mp_obj_t key_tID = mp_obj_new_str_via_qstr(str_key_tID, strlen(str_key_tID));
    mp_obj_t key_state = mp_obj_new_str_via_qstr(str_key_state, strlen(str_key_state));
    mp_obj_t key_mem = mp_obj_new_str_via_qstr(str_key_mem, strlen(str_key_mem));
    mp_obj_t key_thread = mp_obj_new_str_via_qstr(str_key_thread, strlen(str_key_thread));
    mp_obj_t key_args = mp_obj_new_str_via_qstr(str_key_args, strlen(str_key_args));
    mp_obj_t key_status = mp_obj_new_str_via_qstr(str_key_status, strlen(str_key_status));

    mp_obj_t pos;
    if(position < 0){ pos = mp_const_none; }
    else{ pos = mp_obj_new_int(position); }
    mp_obj_t tID = mp_obj_new_int((mp_int_t)context->id);
    mp_obj_t state = mp_obj_new_int((mp_int_t)context->state);
    mp_obj_t mem = mp_obj_new_int((mp_int_t)context->memhead);
    mp_obj_t thread = mp_obj_new_int((mp_int_t)context->threadctrl);
    mp_obj_t args = mp_obj_new_int((mp_int_t)&context->args);
    mp_obj_t status = mp_obj_new_int((mp_int_t)context->status);

    mp_obj_dict_store( context_dict,    key_pos,       pos      );
    mp_obj_dict_store( context_dict,    key_tID,       tID      );
    mp_obj_dict_store( context_dict,    key_state,     state    );
    mp_obj_dict_store( context_dict,    key_mem,       mem      );
    mp_obj_dict_store( context_dict,    key_thread,    thread   );
    mp_obj_dict_store( context_dict,    key_args,      args     );
    mp_obj_dict_store( context_dict,    key_status,    status   );

    return context_dict;
}

void multipython_end_task( uint32_t taskID ){
    xTaskHandle task = (xTaskHandle)taskID;
    mp_context_node_t* context = mp_context_by_tid( taskID );
    if( context == mp_context_head )                { mp_raise_OSError(MP_EACCES); }
    if( ( task == NULL ) || ( context == NULL ) )   { mp_raise_OSError(MP_ENXIO); }
    portENTER_CRITICAL(&mux);
    mp_task_remove( taskID );
    vTaskDelete(task);     
    portEXIT_CRITICAL(&mux);  
}

void multipython_suspend_task( uint32_t taskID ){
    xTaskHandle task = (xTaskHandle)taskID;
    mp_context_node_t* context = mp_context_by_tid( taskID );
    if( context == mp_context_head )                { mp_raise_OSError(MP_EACCES); }
    if( ( task == NULL ) || ( context == NULL ) )   { mp_raise_OSError(MP_ENXIO); }
    context->status |= MP_CSUSP;
    vTaskSuspend(task);
}

int8_t multipython_resume_task( uint32_t taskID ){
    xTaskHandle task = (xTaskHandle)taskID;
    mp_context_node_t* context = mp_context_by_tid( taskID );
    if( context == mp_context_head )                { mp_raise_OSError(MP_EACCES); }
    if( ( task == NULL ) || ( context == NULL ) )   { mp_raise_OSError(MP_ENXIO); }
    if( !(context->status & MP_CSUSP) )             { return -1; }
    context->status &= ~MP_CSUSP;
    vTaskResume(task);
    return(0);
}




// interface 
STATIC mp_obj_t multipython_start(size_t n_args, const mp_obj_t *args) {
    // start new processes (contexts)

    if(n_args == 0){ return mp_const_none; }

    mp_context_node_t* context = mp_task_register( 0, NULL ); // register a task with unknown ID and NULL additional arguments
    if( context == NULL ){
        mp_raise_OSError(MP_ENOMEM);
        return mp_const_none; 
    }

    const char *str = mp_obj_str_get_str(args[0]);
    size_t len = strlen(str) + 1;

    void* source = mp_context_dynmem_alloc( len, context ); // allocate global memory tied to the allocated context
    if( source == NULL ){ 
        mp_context_remove( context );
        mp_raise_OSError(MP_ENOMEM);
        return mp_const_none; 
    }
    memcpy(source, (void*)str, len);
    context->args.input_kind = MP_PARSE_FILE_INPUT;
    context->args.source = source;

    if( n_args == 2 ){
        if( mp_obj_int_get_truncated( args[1] ) == MP_PARSE_SINGLE_INPUT ){
            context->args.input_kind = MP_PARSE_SINGLE_INPUT;
        }
    }

    xTaskCreate(multipython_task_template, "", MULTIPYTHON_TASK_STACK_LEN, (void*)context, MULTIPYTHON_TASK_PRIORITY+1, NULL);
    return mp_const_none;
}
// STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(multipython_start_obj, 1, 2, multipython_start);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(multipython_start_obj, 1, 2, multipython_start);

STATIC mp_obj_t multipython_stop(size_t n_args, const mp_obj_t *args) {
    // stop all processes
    // optionally use a list of tIDs that specify which processes to stop
    // if 'None' is passed as an argument then the current context is stopped
    // returns a list of tIDs for stopped tasks

    mp_context_iter_t iter = NULL;
    mp_obj_t stopped_contexts = mp_obj_new_list( 0, NULL );

    // append items to the list
    switch( n_args ){
        case 1:
            if(mp_obj_is_int(args[0])){
                for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                    if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == mp_obj_int_get_truncated(args[0]) ){
                        multipython_end_task( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id );
                        mp_obj_list_append(stopped_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
                    }
                }
            }else if( mp_obj_is_type(args[0], &mp_type_list) ){
                size_t size;
                mp_obj_t* items;
                mp_obj_list_get(args[0], &size, &items );
                for( size_t indi = 0; indi < size; indi++ ){
                    if( !mp_obj_is_type(items[indi], &mp_type_int) ){
                        mp_raise_TypeError("list element of non-integer type");
                        return mp_const_none;
                    }
                }
                for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                    for( size_t indi = 0; indi < size; indi++ ){
                        if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == mp_obj_int_get_truncated(items[0]) ){
                            multipython_end_task( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id );
                            mp_obj_list_append(stopped_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
                        }
                    }
                }
            }else if( mp_obj_is_type(args[0], &mp_type_NoneType ) ){
                multipython_end_task( (uint32_t)mp_active_context->id );
                mp_obj_list_append(stopped_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
            }else{
                mp_raise_TypeError("expects integer or list of integers");
                return mp_const_none;
            }
            break;

        case 0:
        default:
            for(iter = mp_context_iter_next(mp_context_iter_first(mp_context_head)); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                multipython_end_task( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id );
                mp_obj_list_append(stopped_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
            }
            break;
    }
    return stopped_contexts;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(multipython_stop_obj, 0, 1, multipython_stop);

STATIC mp_obj_t multipython_suspend(size_t n_args, const mp_obj_t *args) {
    // suspend all processes
    // optionally use a list of tIDs that specify which processes to suspend
    // if 'None' is passed as an argument then the current context is suspended
    // returns a list of tIDs for suspended tasks

    mp_context_iter_t iter = NULL;
    mp_obj_t suspended_contexts = mp_obj_new_list( 0, NULL );

    // append items to the list
    switch( n_args ){
        case 1:
            if(mp_obj_is_int(args[0])){
                for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                    if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == mp_obj_int_get_truncated(args[0]) ){
                        multipython_suspend_task( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id );
                        mp_obj_list_append(suspended_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
                    }
                }
            }else if( mp_obj_is_type(args[0], &mp_type_list) ){
                size_t size;
                mp_obj_t* items;
                mp_obj_list_get(args[0], &size, &items );
                for( size_t indi = 0; indi < size; indi++ ){
                    if( !mp_obj_is_type(items[indi], &mp_type_int) ){
                        mp_raise_TypeError("list element of non-integer type");
                        return mp_const_none;
                    }
                }
                for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                    for( size_t indi = 0; indi < size; indi++ ){
                        if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == mp_obj_int_get_truncated(items[0]) ){
                            multipython_suspend_task( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id );
                            mp_obj_list_append(suspended_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
                        }
                    }
                }
            }else if( mp_obj_is_type(args[0], &mp_type_NoneType ) ){
                multipython_suspend_task( (uint32_t)mp_active_context->id );
                mp_obj_list_append(suspended_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
            }else{
                mp_raise_TypeError("expects integer or list of integers");
                return mp_const_none;
            }
            break;

        case 0:
        default:
            for(iter = mp_context_iter_next(mp_context_iter_first(mp_context_head)); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                multipython_suspend_task( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id );
                mp_obj_list_append(suspended_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
            }
            break;
    }
    return suspended_contexts;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(multipython_suspend_obj, 0, 1, multipython_suspend);

STATIC mp_obj_t multipython_resume(size_t n_args, const mp_obj_t *args) {
    // resume all processes
    // optionally use a list of tIDs that specify which processes to resume
    // if 'None' is passed as an argument then the current context is suspended
    // returns a list of tIDs for suspended tasks

    mp_context_iter_t iter = NULL;
    mp_obj_t resumed_contexts = mp_obj_new_list( 0, NULL );

    // append items to the list
    switch( n_args ){
        case 1:
            if(mp_obj_is_int(args[0])){
                for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                    if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == mp_obj_int_get_truncated(args[0]) ){
                        if( multipython_resume_task( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id ) == 0){
                            mp_obj_list_append(resumed_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
                        }
                    }
                }
            }else if( mp_obj_is_type(args[0], &mp_type_list) ){
                size_t size;
                mp_obj_t* items;
                mp_obj_list_get(args[0], &size, &items );
                for( size_t indi = 0; indi < size; indi++ ){
                    if( !mp_obj_is_type(items[indi], &mp_type_int) ){
                        mp_raise_TypeError("list element of non-integer type");
                        return mp_const_none;
                    }
                }
                for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                    for( size_t indi = 0; indi < size; indi++ ){
                        if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == mp_obj_int_get_truncated(items[0]) ){
                            if( multipython_resume_task( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id ) ){
                                mp_obj_list_append(resumed_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
                            }
                        }
                    }
                }
            }else if( mp_obj_is_type(args[0], &mp_type_NoneType ) ){
                // multipython_resume_task( (uint32_t)mp_active_context->id );
                // mp_obj_list_append(resumed_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
            }else{
                mp_raise_TypeError("expects integer or list of integers");
                return mp_const_none;
            }
            break;

        case 0:
        default:
            for(iter = mp_context_iter_next(mp_context_iter_first(mp_context_head)); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                if( multipython_resume_task( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id ) ){
                    mp_obj_list_append(resumed_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
                }
            }
            break;
    }
    return resumed_contexts;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(multipython_resume_obj, 0, 1, multipython_resume);

STATIC mp_obj_t multipython_get(size_t n_args, const mp_obj_t *args) {
    // return a list of dictionaries that represent each context
    // optionally use a list of tIDs for which to return the information as the argument
    // if 'None' is passed as an argument then the information for the current context is returned

    size_t num_entries = 0;
    mp_context_iter_t iter = NULL;
    mp_obj_t context_list = mp_obj_new_list( 0, NULL );

    // append items to the list
    switch( n_args ){
        case 1:
            if(mp_obj_is_int(args[0])){
                for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                    if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == mp_obj_int_get_truncated(args[0]) ){
                        mp_obj_list_append(context_list, multipython_get_context_dict( MP_CONTEXT_PTR_FROM_ITER(iter), num_entries++ ));
                    }
                }
            }else if( mp_obj_is_type(args[0], &mp_type_list) ){
                size_t size;
                mp_obj_t* items;
                mp_obj_list_get(args[0], &size, &items );
                for( size_t indi = 0; indi < size; indi++ ){
                    if( !mp_obj_is_type(items[indi], &mp_type_int) ){
                        mp_raise_TypeError("list element of non-integer type");
                        return mp_const_none;
                    }
                }
                for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                    for( size_t indi = 0; indi < size; indi++ ){
                        if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == mp_obj_int_get_truncated(items[0]) ){
                            mp_obj_list_append(context_list, multipython_get_context_dict( MP_CONTEXT_PTR_FROM_ITER(iter), num_entries++ ));
                        }
                    }
                }
            }else if( mp_obj_is_type(args[0], &mp_type_NoneType ) ){
                mp_obj_list_append(context_list, multipython_get_context_dict( mp_active_context, -1 ));
            }else{
                mp_raise_TypeError("expects integer or list of integers");
                return mp_const_none;
            }
            break;

        case 0:
        default:
            for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                mp_obj_list_append(context_list, multipython_get_context_dict( MP_CONTEXT_PTR_FROM_ITER(iter), num_entries++ ));
            }
            break;
    }
    return context_list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(multipython_get_obj, 0, 1, multipython_get);



// Module Definitions

STATIC const mp_rom_map_elem_t mp_module_multipython_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_multipython) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&multipython_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&multipython_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_suspend), MP_ROM_PTR(&multipython_suspend_obj) },
    { MP_ROM_QSTR(MP_QSTR_resume), MP_ROM_PTR(&multipython_resume_obj) },
    { MP_ROM_QSTR(MP_QSTR_get), MP_ROM_PTR(&multipython_get_obj) },
};
STATIC MP_DEFINE_CONST_DICT(mp_module_multipython_globals, mp_module_multipython_globals_table);

const mp_obj_module_t mp_module_multipython = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_multipython_globals,
};




// multipython task template
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

void multipython_task_template( void* void_context ){
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

    // run boot-up scripts
    pyexec_frozen_module("_boot.py");
    // pyexec_file("boot.py");
    // if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL) {
    //     pyexec_file("main.py");
    // }

    uint8_t error = 0;

    if( context->args.source != NULL ){
        if( context->args.input_kind == MP_PARSE_FILE_INPUT ){
            // pyexec_file( (char*)(context->args.source) );
            if( pyexec_file( (char*)(context->args.source) ) != 1 ){
                error = 1;
            }
            // mp_printf(&mp_plat_print, "pyexec_file returned %d\n", status );
        }else if( context->args.input_kind == MP_PARSE_SINGLE_INPUT){
            if ( execute_from_str( (char*)(context->args.source) ) ){
                error = 1;
            }
        }
    }

    gc_sweep_all();

    mp_deinit();
    fflush(stdout);
    const uint8_t reset = 0;
    
    if(error){ // todo: eventually we will want to be able to catch errors and restart, but allow the task to go to end if it ended of it's own accord
        // vTaskDelay(500/portTICK_PERIOD_MS);
        printf("Error. Current context ID: 0x%x\n", (uint32_t)context->id);
        mp_hal_stdout_tx_str("MPY task: soft restart\r\n"); // todo: add more info saying which task is restarting
        if(reset){
            goto soft_reset;
        }
    }

    portENTER_CRITICAL(&mux);
    mp_task_remove( mp_current_tID );
    portEXIT_CRITICAL(&mux);
    vTaskDelete(NULL);  // When a task deletes itself make sure to release the mux *before* dying
}