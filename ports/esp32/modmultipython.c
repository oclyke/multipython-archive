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

#include "mpstate_spiram.h" // temporary - want to replace this with a general multipython heap allocator file
#include "modmachine.h"

#include <string.h>

// MicroPython runs as a task under FreeRTOS
#define MULTIPYTHON_TASK_PRIORITY        (ESP_TASK_PRIO_MIN + 1)
#define MULTIPYTHON_TASK_STACK_SIZE      (16 * 1024)
#define MULTIPYTHON_TASK_STACK_LEN       (MULTIPYTHON_TASK_STACK_SIZE / sizeof(StackType_t))

#define MULTIPYTHON_CONTEXT_TASK_PRIORITY       (MULTIPYTHON_TASK_PRIORITY + 1)
#define MULTIPYTHON_WAKEUP_CALL_TASK_PRIORITY   (MULTIPYTHON_CONTEXT_TASK_PRIORITY + 1)
#define MULTIPYTHON_WAKEUP_CALL_TASK_PERIOD_MS  (100)

#define MULTIPYTHON_MALLOC(size)        (malloc(size))
#define MULTIPYTHON_FREE(ptr)           (free(ptr))

volatile bool multipython_is_initialized = false;

// types
typedef enum{
    MP_CONTROL_OP_NONE = 0x00,
    MP_CONTROL_OP_STOP,
    MP_CONTROL_OP_RESUME,
    MP_CONTROL_OP_SUSPEND,

    MP_CONTROL_OP_NUM,
}multipython_op_e;

// STATIC mp_obj_t multipython_response_context(mp_obj_t self_in, mp_obj_t context);
// MP_DEFINE_CONST_FUN_OBJ_2(multipython_response_context_obj, multipython_response_context);

// STATIC mp_obj_t multipython_response_operation(mp_obj_t self_in, mp_obj_t operation);
// MP_DEFINE_CONST_FUN_OBJ_2(multipython_response_operation_obj, multipython_response_operation);

STATIC void multipython_response_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind );
mp_obj_t multipython_response_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args );

STATIC const mp_rom_map_elem_t multipython_response_locals_dict_table[] = {
    // { MP_ROM_QSTR(MP_QSTR_context), MP_ROM_PTR(&multipython_response_context_obj) },
    // { MP_ROM_QSTR(MP_QSTR_operation), MP_ROM_PTR(&multipython_response_operation_obj) },
 };
STATIC MP_DEFINE_CONST_DICT(multipython_response_locals_dict, multipython_response_locals_dict_table);

const mp_obj_type_t multipython_responseObj_type = {
    { &mp_type_type },                              // "inherit" the type "type"
    .name = MP_QSTR_responseObj,                    // give it a name
    .print = multipython_response_print,            // give it a print-function
    .make_new = multipython_response_make_new,      // give it a constructor
    .locals_dict = (mp_obj_dict_t*)&multipython_response_locals_dict, // and the global members
};

typedef struct _multipython_response_obj_t {
    mp_obj_base_t               base;       // base represents some basic information, like type
    mp_obj_t                    condition;  // the condition for which this response applies - can be any iterable micropython object such as int, string, list, dict or compound of those
    mp_context_node_t*          context;    // which context to apply the response to
    multipython_op_e            control_op; // a control operation to execute, if any. For example suspend or resume the context
    mp_obj_t                    argument;   // an arbitrary micropython argument that you can place into the queue for the task (useful for providing directions to a context without using the context control operators)                
} multipython_response_obj_t;

typedef struct _multipython_response_node_t multipython_response_node_t;
struct _multipython_response_node_t{
    multipython_response_obj_t*     response;
    multipython_response_node_t*    next;
};

typedef multipython_response_node_t* multipython_response_iter_t;
#define MODADD_RESPONSE_PTR_FROM_ITER(iter)     ((multipython_response_node_t*)iter)
#define MODADD_ITER_FROM_RESPONSE_PTR(ptr)      ((multipython_response_iter_t)ptr)

multipython_response_iter_t multipython_response_iter_first( multipython_response_iter_t head ){ return head; }
bool multipython_response_iter_done( multipython_response_iter_t iter ){ return (iter == NULL); }
multipython_response_iter_t multipython_response_iter_next( multipython_response_iter_t iter ){ return (MODADD_RESPONSE_PTR_FROM_ITER(iter)->next); }
void multipython_response_foreach(multipython_response_iter_t head, void (*f)(multipython_response_iter_t iter, void*), void* args){
    multipython_response_iter_t iter = NULL;
    for( iter = multipython_response_iter_first(head); !multipython_response_iter_done(iter); iter = multipython_response_iter_next(iter) ){ 
        f(iter, args); 
    }
}


// globals
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
multipython_response_node_t* multipython_response_head = NULL;

// Forward declarations
mp_obj_t execute_from_str(const char *str);
void multipython_task_template( void* void_context );
void multipython_wakeup_call_task( void* args );

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
    const char* str_key_context = "context_address";

    mp_obj_t key_pos = mp_obj_new_str_via_qstr(str_key_pos, strlen(str_key_pos));
    mp_obj_t key_tID = mp_obj_new_str_via_qstr(str_key_tID, strlen(str_key_tID));
    mp_obj_t key_state = mp_obj_new_str_via_qstr(str_key_state, strlen(str_key_state));
    mp_obj_t key_mem = mp_obj_new_str_via_qstr(str_key_mem, strlen(str_key_mem));
    mp_obj_t key_thread = mp_obj_new_str_via_qstr(str_key_thread, strlen(str_key_thread));
    mp_obj_t key_args = mp_obj_new_str_via_qstr(str_key_args, strlen(str_key_args));
    mp_obj_t key_status = mp_obj_new_str_via_qstr(str_key_status, strlen(str_key_status));
    mp_obj_t key_context = mp_obj_new_str_via_qstr(str_key_context, strlen(str_key_context));

    mp_obj_t pos;
    if(position < 0){ pos = mp_const_none; }
    else{ pos = mp_obj_new_int(position); }
    mp_obj_t tID = mp_obj_new_int((mp_int_t)context->id);
    mp_obj_t state = mp_obj_new_int((mp_int_t)context->state);
    mp_obj_t mem = mp_obj_new_int((mp_int_t)context->memhead);
    mp_obj_t thread = mp_obj_new_int((mp_int_t)context->threadctrl);
    mp_obj_t args = mp_obj_new_int((mp_int_t)&context->args);
    mp_obj_t status = mp_obj_new_int((mp_int_t)context->status);
    mp_obj_t context_obj = mp_obj_new_int((mp_int_t)context);

    mp_obj_dict_store( context_dict,    key_pos,       pos      );
    mp_obj_dict_store( context_dict,    key_tID,       tID      );
    mp_obj_dict_store( context_dict,    key_state,     state    );
    mp_obj_dict_store( context_dict,    key_mem,       mem      );
    mp_obj_dict_store( context_dict,    key_thread,    thread   );
    mp_obj_dict_store( context_dict,    key_args,      args     );
    mp_obj_dict_store( context_dict,    key_status,    status   );
    mp_obj_dict_store( context_dict,    key_context,   context_obj   );

    return context_dict;
}


typedef int8_t (*multipython_control_f)( uint32_t taskID );

int8_t multipython_end_task( uint32_t taskID ){
    xTaskHandle task = (xTaskHandle)taskID;
    mp_context_node_t* context = mp_context_by_tid( taskID );
    if( context == mp_context_head )                { mp_raise_OSError(MP_EACCES); }
    if( ( task == NULL ) || ( context == NULL ) )   { mp_raise_OSError(MP_ENXIO); }
    portENTER_CRITICAL(&mux);
    mp_task_remove( taskID );
    vTaskDelete(task);     
    portEXIT_CRITICAL(&mux);  
    return 0;
}

int8_t multipython_suspend_task( uint32_t taskID ){
    xTaskHandle task = (xTaskHandle)taskID;
    mp_context_node_t* context = mp_context_by_tid( taskID );
    if( context == mp_context_head )                { mp_raise_OSError(MP_EACCES); }
    if( ( task == NULL ) || ( context == NULL ) )   { mp_raise_OSError(MP_ENXIO); }
    context->status |= MP_CSUSP;
    vTaskSuspend(task);
    return 0;
}

int8_t multipython_resume_task( uint32_t taskID ){
    xTaskHandle task = (xTaskHandle)taskID;
    mp_context_node_t* context = mp_context_by_tid( taskID );
    if( context == mp_context_head )                { mp_raise_OSError(MP_EACCES); }
    if( ( task == NULL ) || ( context == NULL ) )   { mp_raise_OSError(MP_ENXIO); }
    if( !(context->status & MP_CSUSP) )             { return -1; }
    context->status &= ~MP_CSUSP;
    vTaskResume(task);
    return 0;
}




// interface 
STATIC mp_obj_t multipython_start(size_t n_args, const mp_obj_t *args) { // todo: allow to set priority and heap size!
    // start new processes (contexts)
    enum { ARG_source, ARG_type, ARG_core, ARG_suspend };

    if(n_args == 0){ return mp_const_none; }

    mp_context_node_t* context = mp_task_register( 0, NULL ); // register a task with unknown ID and NULL additional arguments
    if( context == NULL ){
        mp_raise_OSError(MP_ENOMEM);
        return mp_const_none; 
    }

    const char *str = mp_obj_str_get_str(args[ARG_source]);
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
    context->args.suspend = 0;
    if( n_args > 3 ){
        context->args.suspend = mp_obj_int_get_truncated( args[ARG_suspend] );
    }

    if( n_args >1 ){
        if( mp_obj_int_get_truncated( args[ARG_type] ) == MP_PARSE_SINGLE_INPUT ){
            context->args.input_kind = MP_PARSE_SINGLE_INPUT;
        }
    }

    
    if( n_args > 2 ){
        uint8_t core = 0;
        core = mp_obj_int_get_truncated( args[ARG_core] );
        if( core > 1 ){
            core = 1;
        }
        xTaskCreatePinnedToCore( multipython_task_template, "", MULTIPYTHON_TASK_STACK_LEN, (void*)context, MULTIPYTHON_CONTEXT_TASK_PRIORITY, NULL, core );
        //             /* Function to implement the task */
        //                                        /* Name of the task */
        //                                                       /* Stack size in words */
        //                                                                              /* Task input parameter */
        //                                                                                                      /* Priority of the task */
        //                                                                                                                          /* Task handle. */
        //                                                                                                                      /* Core where the task should run */
    }else{
        xTaskCreate(multipython_task_template, "", MULTIPYTHON_TASK_STACK_LEN, (void*)context, MULTIPYTHON_CONTEXT_TASK_PRIORITY, NULL); // core determined by FreeRTOS
    }    
    
    return MP_OBJ_NEW_SMALL_INT((uint32_t)context);
}
// STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(multipython_start_obj, 1, 2, multipython_start);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(multipython_start_obj, 1, 4, multipython_start);

STATIC mp_obj_t _multipython_control(uint32_t id, multipython_op_e op, bool use_context ){
    // todo: should get rid of this internal version and actually figure out how to call KW functions (like the real 'control' function) from C

    // todo: major cleanup of the condition/response system!

    // determine the operation
    multipython_control_f op_func = NULL;
    switch( op ){
        case MP_CONTROL_OP_STOP :       op_func = multipython_end_task;     break; // todo: rename 'stop' to 'end' b/c it is more dramatic
        case MP_CONTROL_OP_RESUME :     op_func = multipython_resume_task;  break;
        case MP_CONTROL_OP_SUSPEND :    op_func = multipython_suspend_task; break;

        case MP_CONTROL_OP_NONE :
        case MP_CONTROL_OP_NUM :
            mp_raise_msg(&mp_type_OSError, "Operation not supported\n");
            return mp_const_none; 
            break;
    }

    // printf("here2\n");

    mp_context_iter_t iter = NULL;
    mp_obj_t controlled_contexts = mp_obj_new_list( 0, NULL );

    // // append items to the list
    // if(n_args == 0){    // with 0 arguments all contexts are affected
    //     for(iter = mp_context_iter_next(mp_context_iter_first(mp_context_head)); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
    //         if( op_func( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id ) == 0 ){
    //             mp_obj_list_append(controlled_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
    //         }
    //     }
    // }else{
        if(1){
            uint32_t target_id = id;
            if( use_context ){
                // printf("we ARE using the context, and the id is %d\n",target_id);

                mp_context_node_t* node = (mp_context_node_t*)id;
                target_id = node->id;
            }
            for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                
                if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == target_id ){
                    int8_t result = 0xFF;
                    result = op_func( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id ); 
                    // printf("just ran the operation function: result = %d", result);

                    // if( op_func( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id ) == 0 ){

                        // mp_obj_list_append(controlled_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
                    // }
                }
            }
        // }else if( mp_obj_is_type(ids, &mp_type_list) ){
        //     printf("ids is a list?\n");
        //     size_t size;
        //     mp_obj_t* items;
        //     mp_obj_list_get(ids, &size, &items );
        //     for( size_t indi = 0; indi < size; indi++ ){
        //         if( !mp_obj_is_int(items[indi]) ){
        //             mp_raise_TypeError("list element of non-integer type");
        //             return mp_const_none;
        //         }
        //     }
        //     for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
        //         for( size_t indi = 0; indi < size; indi++ ){
        //             mp_int_t id = mp_obj_int_get_truncated(items[indi]);
        //             uint32_t target_id = (uint32_t)id;
        //             if( use_context ){
        //                 mp_context_node_t* node = (mp_context_node_t*)id;
        //                 target_id = node->id;
        //             }
        //             if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == target_id ){
        //                 if( op_func( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id ) == 0 ){
        //                     mp_obj_list_append(controlled_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
        //                 }
        //             }
        //         }
        //     }
        // }else if( mp_obj_is_type(ids, &mp_type_NoneType ) ){
        //     printf("oh.. ids is NONE type\n");
        //     if( op_func( (uint32_t)mp_active_contexts[MICROPY_GET_CORE_INDEX]->id ) == 0 ){
        //         mp_obj_list_append(controlled_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
        //     }
        // }else{
        //     mp_raise_TypeError("expects integer or list of integers");
        //     return mp_const_none;
        }
    // }
    return controlled_contexts;
}

STATIC mp_obj_t multipython_control(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args){
    // apply an operation to specified processes
    // specify which processes to affect by:
    // - providing the argument 'ids' as None to affect the calling process
    // - providing an integer, or list of integers
    //   - if 'use_context' is false then the 'ids' correspond to taskIDs                                       // todo: remove tID from multipython interface. Users should deal only with the id of the context as the address of the multipython context node.
    //   - if 'use_context' is true then the 'ids' correspond to pointers to the multipython context objects
    // specify the operation as one of the 'multipython_op_e' values

    // typically returns a list of tIDs that were affected                                                      // todo: change API to simply always return the micropython object correspinding to the context. that means making contexts into a micropython object type
    // when the operation is 'GET' returns a list of dictionaries containing information about active contexts.

    enum { ARG_ids, ARG_op, ARG_use_context};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_ids,          MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_op,           MP_ARG_INT,                     {.u_int = MP_CONTROL_OP_NONE } },
        { MP_QSTR_use_context,  MP_ARG_OBJ,                     {.u_obj = mp_const_false} },
    };
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // determine the operation
    multipython_control_f op_func = NULL;
    switch( args[ARG_op].u_int ){
        case MP_CONTROL_OP_STOP :       op_func = multipython_end_task;     /*printf("operation: stop\n");*/        break; // todo: rename 'stop' to 'end' b/c it is more dramatic
        case MP_CONTROL_OP_RESUME :     op_func = multipython_resume_task;  /*printf("operation: resume\n");*/      break;
        case MP_CONTROL_OP_SUSPEND :    op_func = multipython_suspend_task; /*printf("operation: suspend\n");*/     break;

        case MP_CONTROL_OP_NONE :
        case MP_CONTROL_OP_NUM :
            mp_raise_msg(&mp_type_OSError, "Operation not supported\n");
            return mp_const_none; 
            break;
    }

    mp_context_iter_t iter = NULL;
    mp_obj_t controlled_contexts = mp_obj_new_list( 0, NULL );

    // append items to the list
    if(n_args == 0){    // with 0 arguments all contexts are affected
        for(iter = mp_context_iter_next(mp_context_iter_first(mp_context_head)); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
            if( op_func( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id ) == 0 ){
                mp_obj_list_append(controlled_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
            }
        }
    }else{
        if(mp_obj_is_int(args[ARG_ids].u_obj)){
            mp_int_t id = mp_obj_int_get_truncated(args[ARG_ids].u_obj);
            uint32_t target_id = (uint32_t)id;
            if( mp_obj_is_true( args[ARG_use_context].u_obj ) ){
                mp_context_node_t* node = (mp_context_node_t*)id;
                target_id = node->id;
            }
            for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == target_id ){
                    if( op_func( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id ) == 0 ){
                        mp_obj_list_append(controlled_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
                    }
                }
            }
        }else if( mp_obj_is_type(args[ARG_ids].u_obj, &mp_type_list) ){
            size_t size;
            mp_obj_t* items;
            mp_obj_list_get(args[ARG_ids].u_obj, &size, &items );
            for( size_t indi = 0; indi < size; indi++ ){
                if( !mp_obj_is_int(items[indi]) ){
                    mp_raise_TypeError("list element of non-integer type");
                    return mp_const_none;
                }
            }
            for(iter = mp_context_iter_first(mp_context_head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
                for( size_t indi = 0; indi < size; indi++ ){
                    mp_int_t id = mp_obj_int_get_truncated(items[indi]);
                    uint32_t target_id = (uint32_t)id;
                    if( mp_obj_is_true( args[ARG_use_context].u_obj ) ){
                        mp_context_node_t* node = (mp_context_node_t*)id;
                        target_id = node->id;
                    }
                    if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == target_id ){
                        if( op_func( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id ) == 0 ){
                            mp_obj_list_append(controlled_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
                        }
                    }
                }
            }
        }else if( mp_obj_is_type(args[ARG_ids].u_obj, &mp_type_NoneType ) ){
            if( op_func( (uint32_t)mp_active_contexts[MICROPY_GET_CORE_INDEX]->id ) == 0 ){
                mp_obj_list_append(controlled_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
            }
        }else{
            mp_raise_TypeError("expects integer or list of integers");
            return mp_const_none;
        }
    }
    return controlled_contexts;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(multipython_control_obj, 0, multipython_control);

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
                        if( MP_CONTEXT_PTR_FROM_ITER(iter)->id == mp_obj_int_get_truncated(items[indi]) ){
                            mp_obj_list_append(context_list, multipython_get_context_dict( MP_CONTEXT_PTR_FROM_ITER(iter), num_entries++ ));
                        }
                    }
                }
            }else if( mp_obj_is_type(args[0], &mp_type_NoneType ) ){
                mp_obj_list_append(context_list, multipython_get_context_dict( mp_active_contexts[MICROPY_GET_CORE_INDEX], -1 ));
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

STATIC mp_obj_t multipython_get_task_id(mp_obj_t context_ptr_obj) {
    // return the taskID for the task whose context is at the address pointed to by the context ptr number

    // todo: make this function default to returning the tID of the executing process, if no arguments supplied

    if( !mp_obj_is_int(context_ptr_obj) ){
        return mp_const_none;
    }

    mp_context_node_t* context = (mp_context_node_t*)mp_obj_int_get_truncated( context_ptr_obj );
    mp_context_iter_t iter = NULL;

    if( context == NULL ){
        return mp_const_none;
    }

    for( iter = mp_context_iter_first(MP_ITER_FROM_CONTEXT_PTR(mp_context_head)); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter) ){
        if(MP_CONTEXT_PTR_FROM_ITER(iter) == context){ break; }
    }

    return MP_OBJ_NEW_SMALL_INT((uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(multipython_get_task_id_obj, multipython_get_task_id);

IRAM_ATTR mp_obj_t multipython_notify(mp_obj_t condition) { // notify any applicable tasks about a given condition
    if( mp_obj_equal(condition, mp_const_none) ){ return mp_const_none; }

    // loop through the linked list of responses and see if any match the condition
    multipython_response_iter_t iter = NULL;
    for( iter = multipython_response_iter_first(multipython_response_head); !multipython_response_iter_done(iter); iter = multipython_response_iter_next(iter) ){ 
        multipython_response_obj_t* response = MODADD_RESPONSE_PTR_FROM_ITER(iter)->response;
        if( response == NULL ){             // this would represent a problem...
            mp_raise_msg(&mp_type_OSError, "Encountered a response node w/o response. This is not good\n");
            continue; 
        }
        if( mp_obj_equal(response->condition, condition) ){
            // printf("Node: 0x%8X - Matched\n", (uint32_t)iter);
            mp_response_t response_entry = {
                .control_op = response->control_op,
                .argument = response->argument,
            };
            size_t num_written = mp_response_queue_write( &(response->context->response_queue), response_entry );
        }else{
            // printf("Node: 0x%8X - No Match\n", (uint32_t)iter);
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(multipython_notify_obj, multipython_notify);

STATIC mp_obj_t multipython_check_responses( void ) {
    // check if there are any pending operations for the calling process 

    mp_response_t response;
    size_t num_read = mp_response_queue_read(&(mp_active_contexts[MICROPY_GET_CORE_INDEX]->response_queue), &response);
    if( num_read != 1){
        return mp_const_none;
    }

    // // execute the control operation, if any
    // if( response.control_op != MP_CONTROL_OP_NONE ){
    //     _multipython_control( (uint32_t)mp_active_contexts[MICROPY_GET_CORE_INDEX], response.control_op, true );
    // }

    return response.argument;    
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_check_responses_obj, multipython_check_responses);

STATIC mp_obj_t multipython_context( void ) {
    // return the context address of calling process
    return MP_OBJ_NEW_SMALL_INT((uint32_t)mp_active_contexts[MICROPY_GET_CORE_INDEX]);    
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_context_obj, multipython_context);

STATIC mp_obj_t multipython_init( void ){
    if(multipython_is_initialized == true){ return mp_const_none; }
    
    xTaskCreatePinnedToCore( multipython_wakeup_call_task, "multipython wakeup call task", MULTIPYTHON_TASK_STACK_LEN, NULL, MULTIPYTHON_WAKEUP_CALL_TASK_PRIORITY, NULL, MICROPY_REPL_CORE );

    multipython_is_initialized = true;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(multipython_init_obj, multipython_init);


// Module Definitions
STATIC const mp_rom_map_elem_t mp_module_multipython_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_multipython) },

    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&multipython_start_obj) },                 // processes
    { MP_ROM_QSTR(MP_QSTR_control), MP_ROM_PTR(&multipython_control_obj) },
    { MP_ROM_QSTR(MP_QSTR_get), MP_ROM_PTR(&multipython_get_obj) },
    { MP_ROM_QSTR(MP_QSTR_check_responses), MP_ROM_PTR(&multipython_check_responses_obj) },
    
    
    { MP_ROM_QSTR(MP_QSTR_get_tID), MP_ROM_PTR(&multipython_get_task_id_obj) },         // todo: switch to using only the address of context nodes as identifiers within multipython
    { MP_ROM_QSTR(MP_QSTR_notify), MP_ROM_PTR(&multipython_notify_obj) },               // utility
    { MP_ROM_QSTR(MP_QSTR_context), MP_ROM_PTR(&multipython_context_obj) }, 
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&multipython_init_obj) },                           // initializes multipython (mostly this means starting the wakeup call task)
    
    
    { MP_ROM_QSTR(MP_QSTR_response),     MP_ROM_PTR(&multipython_responseObj_type) },   // response objects

    { MP_ROM_QSTR(MP_QSTR_CONTROL_NONE), MP_ROM_INT(MP_CONTROL_OP_NONE) },           // control operations
    { MP_ROM_QSTR(MP_QSTR_CONTROL_STOP), MP_ROM_INT(MP_CONTROL_OP_STOP) },
    { MP_ROM_QSTR(MP_QSTR_CONTROL_RESUME), MP_ROM_INT(MP_CONTROL_OP_RESUME) },
    { MP_ROM_QSTR(MP_QSTR_CONTROL_SUSPEND), MP_ROM_INT(MP_CONTROL_OP_SUSPEND) },
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
    context->id = mp_current_tIDs[MICROPY_GET_CORE_INDEX];
    mp_context_switch(context);
    
    volatile uint32_t sp = (uint32_t)get_sp();
    #if MICROPY_PY_THREAD
    mp_thread_init(pxTaskGetStackStart(NULL), MULTIPYTHON_TASK_STACK_LEN);
    #endif
    // uart_init();

    void* mp_task_heap = NULL;
    size_t mp_task_heap_size = 0;
    #if CONFIG_SPIRAM_SUPPORT
    switch (esp_spiram_get_chip_size()) {
        case ESP_SPIRAM_SIZE_16MBITS:
            // mp_task_heap_size = 2 * 1024 * 1024;
            mp_task_heap_size = 1024*200; // temporarily hard-coded at ~1/10th of available SPIRAM. One day will make this customizable, then one day will make it dynamically increasable as needed
            break;
        case ESP_SPIRAM_SIZE_32MBITS:
        case ESP_SPIRAM_SIZE_64MBITS:   // 8 MB needs special API to access upper 4MB, so for now cap at 4 MB heap
            // mp_task_heap_size = 4 * 1024 * 1024;
            // mp_task_heap_size = 400000;
            mp_task_heap_size = 1024*200; // ~1/200 of available memory - should allow plenty of tasks! needs to be 4-byte aligned
            break;
        default:
            // // No SPIRAM, fallback to normal allocation
            // mp_task_heap_size = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
            mp_task_heap_size = 32 * 1024; // todo: temporary trying to reduce heap usage
            // size_t mp_task_heap_size = 10000; // todo: temporary trying to reduce heap usage
            // mp_task_heap = mp_task_alloc( mp_task_heap_size, mp_current_tIDs[MICROPY_GET_CORE_INDEX] ); // todo: allow the task to decide on the GC heap size (user input or something)
            break;
    }
    mp_task_heap = mp_task_alloc_heap_caps( mp_task_heap_size, mp_current_tIDs[MICROPY_GET_CORE_INDEX], MALLOC_CAP_SPIRAM );
    // printf("mp_task_heap ptr: 0x%X, size = 0x%X\n", (uint32_t)mp_task_heap, mp_task_heap_size );
    #else
    // // Allocate the uPy heap using mp_task_alloc and get the largest available region
    // size_t mp_task_heap_size = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    mp_task_heap_size = 32 * 1024; // todo: make this not hardcoded
    mp_task_heap = mp_task_alloc( mp_task_heap_size, mp_current_tIDs[MICROPY_GET_CORE_INDEX] ); // todo: allow the task to decide on the GC heap size (user input or something)
    #endif
    if( mp_task_heap == NULL ){
        printf("Could not allocate memory for task. aborting\n");
        goto remove_task;
    }

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

    // Option to suspend the task at startup
    if( context->args.suspend ){
        context->status |= MP_CSUSP;
        vTaskSuspend(NULL);
    }

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

remove_task:

    portENTER_CRITICAL(&mux);
    mp_task_remove( mp_current_tIDs[MICROPY_GET_CORE_INDEX] );
    portEXIT_CRITICAL(&mux);
    vTaskDelete(NULL);  // When a task deletes itself make sure to release the mux *before* dying
}

void multipython_wakeup_call_task( void* args ){
    for(;;){
        vTaskDelay(MULTIPYTHON_WAKEUP_CALL_TASK_PERIOD_MS/portTICK_PERIOD_MS);
        // printf("wakey wakey eggs and bakey\n");

        mp_context_iter_t citer = NULL;
        for(citer = mp_context_iter_first(mp_context_head); !mp_context_iter_done(citer); citer = mp_context_iter_next(citer)){
            mp_response_t response = {0};
            // printf("context: 0x%08X\n", (uint32_t)citer );

            size_t available = mp_response_queue_peek(&MP_CONTEXT_PTR_FROM_ITER(citer)->response_queue, &response);
            if( available != 0){
                // printf("we got a live one ooohwee! context = 0x%08X, control_op = %d, arg = 0x%08X\n",(uint32_t)citer, response.control_op, (uint32_t)response.argument);

                // there is a response in the cue for this task. If it is a resume task then it should be handled here
                if( response.control_op == MP_CONTROL_OP_RESUME ){
                    _multipython_control( (uint32_t)citer, response.control_op, true );
                }
            }
        }
    }
    // this task should never quit
}












// response type functions
STATIC void multipython_response_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind ) {
    // addressable_fixture_obj_t *self = MP_OBJ_TO_PTR(self_in);
    printf ("multipython response class object:\n");
}

mp_obj_t multipython_response_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args ) {
    enum { ARG_condition, ARG_context, ARG_control_op, ARG_argument };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_condition,    MP_ARG_OBJ,    {.u_obj = mp_const_none} },
        { MP_QSTR_context,      MP_ARG_INT,    {.u_int = 0} },
        { MP_QSTR_control_op,   MP_ARG_INT,    {.u_int = MP_CONTROL_OP_NONE} },
        { MP_QSTR_argument,     MP_ARG_OBJ,    {.u_obj = mp_const_none} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    
    // // this checks the number of arguments (min 1, max 1);
    // // on error -> raise python exception
    // mp_arg_check_num(n_args, n_kw, 1, 2, true);

    // Instead of using micropython's allocation here we will use the system's, to ensure(?) that they have global scope and lifetime for access from any process
    multipython_response_obj_t* response = (multipython_response_obj_t*)MULTIPYTHON_MALLOC(1*sizeof(multipython_response_obj_t));
    if( response == NULL ){
        mp_raise_OSError(MP_ENOMEM);
        return mp_const_none;
    }

    // If that succeeded allocate a new node:
    multipython_response_node_t* node = (multipython_response_node_t*)MULTIPYTHON_MALLOC(1*sizeof(multipython_response_node_t));
    if( node == NULL ){
        MULTIPYTHON_FREE(response);
        mp_raise_OSError(MP_ENOMEM);
        return mp_const_none;
    }

    // If that succeeded then fill out the node and response:
    node->response = response;
    node->next = NULL;

    memset( (void*)response, 0x00, sizeof(multipython_response_obj_t) );
    response->base.type = &multipython_responseObj_type; // give it a type
    response->condition = args[ARG_condition].u_obj;
    response->context = (mp_context_node_t*)args[ARG_context].u_int;
    response->control_op = (multipython_op_e)args[ARG_control_op].u_int;
    response->argument = args[ARG_argument].u_obj;

    // Then link the node into the LL
    if( multipython_response_head == NULL ){
        multipython_response_head = node;
        // printf("Added node to the head location\n");
    }else{
        multipython_response_iter_t iter = NULL;
        for( iter = multipython_response_iter_first(multipython_response_head); !multipython_response_iter_done(iter); iter = multipython_response_iter_next(iter) ){ 
            if( MODADD_RESPONSE_PTR_FROM_ITER(iter)->next == NULL ){ break; }
        }
        MODADD_RESPONSE_PTR_FROM_ITER(iter)->next = node;
        // printf("Added node to the tail of the LL\n");
    }

    return response;
}

// STATIC mp_obj_t multipython_response_context(mp_obj_t self_in, mp_obj_t context){
//     multipython_response_obj_t *self = MP_OBJ_TO_PTR(self_in);
//     if(mp_obj_is_int(context)){
//         self->context = (mp_context_node_t*)mp_obj_get_int(context);
//     }
//     return mp_const_none;
// }

// STATIC mp_obj_t multipython_response_operation(mp_obj_t self_in, mp_obj_t operation){
//     multipython_response_obj_t *self = MP_OBJ_TO_PTR(self_in);
//     if(mp_obj_is_int(operation)){
//         self->operation = (multipython_op_e)mp_obj_get_int(operation);
//     }
//     return mp_const_none;
// }