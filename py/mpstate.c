/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/mpstate.h"
#include <string.h>
#include <stdlib.h>

#define MP_STATE_MALLOC(size) (malloc(size))
#define MP_STATE_FREE(ptr) (free(ptr))

#if MICROPY_DYNAMIC_COMPILER
mp_dynamic_compiler_t mp_dynamic_compiler = {0};
#endif

void mp_context_switch(mp_context_node_t* node){
    if( node == NULL ){ return; }
    mp_active_context = *(node);
    mp_context_refresh();
}

void mp_context_refresh( void ){
    mp_active_dict_main = MP_STATE_VM(dict_main);
    #if MICROPY_PY_SYS
    mp_active_loaded_modules_dict = MP_STATE_VM(mp_loaded_modules_dict);
    memcpy((void*)&mp_active_sys_path_obj, (void*)&MP_STATE_VM(mp_sys_path_obj), sizeof(mp_obj_list_t));
    memcpy((void*)&mp_active_sys_argv_obj, (void*)&MP_STATE_VM(mp_sys_argv_obj), sizeof(mp_obj_list_t));
    #endif // MICROPY_PY_SYS
}

// context iterator functions
mp_context_iter_t mp_context_iter_first( mp_context_iter_t head ){ return head; }
bool mp_context_iter_done( mp_context_iter_t iter ){ return (iter == NULL); }
mp_context_iter_t mp_context_iter_next( mp_context_iter_t iter ){ return ((mp_context_iter_t)iter->next); }
void mp_context_foreach(mp_context_iter_t head, void (*f)(mp_context_iter_t iter, void*), void* args){
    mp_context_iter_t iter = NULL;
    for( iter = mp_context_iter_first(head); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter) ){ 
        f(iter, args); 
    }
}

// context linked list manipulation
mp_state_ctx_t* mp_new_state( void ){
    mp_state_ctx_t* state = NULL;
    state = (mp_state_ctx_t*)MP_STATE_MALLOC(1*sizeof(mp_state_ctx_t));
    if(state == NULL){ return state; }
    memset((void*)state, 0x00, sizeof(mp_state_ctx_t));
    return state;
}

mp_context_node_t* mp_new_context_node( void ){
    mp_context_node_t* node = NULL;
    node = (mp_context_node_t*)MP_STATE_MALLOC(1*sizeof(mp_context_node_t));
    if(node == NULL){ return node; }
    memset((void*)node, 0x00, sizeof(mp_context_node_t));
    return node;
}

mp_context_node_t* mp_context_by_tid( uint32_t tID ){
    mp_context_iter_t iter = NULL;
    for( iter = mp_context_iter_first(MP_ITER_FROM_CONTEXT_PTR(mp_context_head)); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter) ){
        if(MP_CONTEXT_PTR_FROM_ITER(iter)->id == tID){ break; }
    }
    return MP_CONTEXT_PTR_FROM_ITER(iter);
}

mp_context_node_t* mp_context_predecessor( mp_context_node_t* successor ){
    mp_context_iter_t iter = NULL;
    for( iter = mp_context_iter_first(MP_ITER_FROM_CONTEXT_PTR(mp_context_head)); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter) ){
        if(MP_CONTEXT_PTR_FROM_ITER(iter)->next == successor){ break; }
    }
    return MP_CONTEXT_PTR_FROM_ITER(iter);
}

mp_context_node_t* mp_context_tail( void ){
    return mp_context_predecessor(NULL);
}

void mp_context_append( mp_context_node_t* node ){
    if(node == NULL){ return; }
    mp_context_node_t* tail = mp_context_tail();
    if(tail == NULL){ return; }
    tail->next = node;
    node->next = NULL;
}

void mp_context_remove( mp_context_node_t* node ){
    mp_context_node_t* predecessor = mp_context_predecessor(node);
    if( predecessor == NULL ){ return; }
    mp_context_node_t* successor = NULL;
    successor = node->next;
    predecessor->next = successor;
    MP_STATE_FREE(node->state);
    MP_STATE_FREE(node);
}

void mp_task_register( uint32_t tID, void* args ){
    mp_context_node_t* node = NULL;
    node = mp_context_by_tid( tID );
    if( node != NULL ){ return; } // can't register the same task ID
    node = mp_new_context_node();
    if( node == NULL ){ return; } // no heap
    mp_state_ctx_t* state = mp_new_state();
    if( state == NULL ){ 
        MP_STATE_FREE(node);
        return; // no heap
    }
    node->id = tID;
    node->state = state;
    node->args = args;
    node->next = NULL;
    mp_context_append(node);
}

void mp_task_remove( uint32_t tID ){
    mp_context_node_t* node = NULL;
    node = mp_context_by_tid( tID );
    if( node == NULL ){ return; }
    mp_context_remove(node);
}

void mp_task_switched_in( uint32_t tID ){
    mp_context_node_t* node = mp_context_by_tid( tID );
    if(node == NULL){ return; }
    mp_context_switch(node);
}

// globals
mp_state_ctx_t _hidden_mp_state_ctx;

mp_context_node_t mp_default_context = {
    .id = 0,
    .state = &_hidden_mp_state_ctx,
    .next = NULL,
};

mp_context_node_t mp_active_context;
mp_context_node_t* mp_context_head = &mp_default_context;volatile uint32_t mp_current_tID = NULL;
