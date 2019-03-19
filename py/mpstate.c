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

#if MICROPY_DYNAMIC_COMPILER
mp_dynamic_compiler_t mp_dynamic_compiler = {0};
#endif

// Forward declarartions (private)
int8_t mp_task_free_all( uint32_t tID );


// switch the micropython state to a given node
void mp_context_switch(mp_context_node_t* node){
    if( node == NULL ){ return; }
    mp_active_contexts[MICROPY_GET_CORE_INDEX] = node;
    mp_active_context_mirrors[MICROPY_GET_CORE_INDEX] = *(mp_active_contexts[MICROPY_GET_CORE_INDEX]);
    mp_context_refresh();
}

// refresh mirror variables
void mp_context_refresh( void ){
    mp_active_dict_mains[MICROPY_GET_CORE_INDEX] = MP_STATE_VM(dict_main);
    #if MICROPY_PY_SYS
    mp_active_loaded_modules_dicts[MICROPY_GET_CORE_INDEX] = MP_STATE_VM(mp_loaded_modules_dict);
    memcpy((void*)&(mp_active_sys_path_objs[MICROPY_GET_CORE_INDEX]), (void*)&MP_STATE_VM(mp_sys_path_obj), sizeof(mp_obj_list_t));
    memcpy((void*)&(mp_active_sys_argv_objs[MICROPY_GET_CORE_INDEX]), (void*)&MP_STATE_VM(mp_sys_argv_obj), sizeof(mp_obj_list_t));
    #endif // MICROPY_PY_SYS
}

// operation queue functions

size_t mp_response_queue_available( mp_response_queue_t* queue ){
    if( queue->r_head == queue->w_head ){ return 0; }
    if( queue->w_head > queue->r_head ){ return (queue->w_head - queue->r_head); }
    else{ return (( queue->w_head ) + ( MP_RESPONSE_QUEUE_SIZE - 1 - queue->r_head ) ); }
}

size_t mp_response_queue_peek( mp_response_queue_t* queue, mp_response_t* response ){
    if( queue->r_head == queue->w_head ){ return 0; }
    *response = *(queue->buff + queue->r_head);
    return 1;
}

size_t mp_response_queue_read( mp_response_queue_t* queue, mp_response_t* response ){
    size_t next = 0;
    if( queue->r_head == queue->w_head ){ return 0; }
    next = queue->r_head + 1;
    if( next >= MP_RESPONSE_QUEUE_SIZE ){ next = 0; }
    *response = *(queue->buff + queue->r_head);
    queue->r_head = next;
    return 1;
}

size_t mp_response_queue_write( mp_response_queue_t* queue, mp_response_t response ){
    // stores an operation in the operation queue, unless the queue is full. Returns the number of successful stores
    size_t next = queue->w_head + 1;
    if( next >= MP_RESPONSE_QUEUE_SIZE ){ next = 0; }
    if( next == queue->r_head ){ return 0; }
    *(queue->buff + queue->w_head) = response;
    queue->w_head = next;
    return 1;
}

// context iterator functions
mp_context_iter_t mp_context_iter_first( mp_context_iter_t head ){ return head; }
bool mp_context_iter_done( mp_context_iter_t iter ){ return (iter == NULL); }
mp_context_iter_t mp_context_iter_next( mp_context_iter_t iter ){ return (MP_CONTEXT_PTR_FROM_ITER(iter)->next); }
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

mp_context_node_t* mp_context_append_new( void ){
    mp_context_node_t* node = NULL;
    node = mp_new_context_node();
    if( node == NULL ){ return NULL; } // no heap
    mp_state_ctx_t* state = mp_new_state();
    if( state == NULL ){ 
        MP_STATE_FREE(node);
        return NULL; // no heap
    }
    node->state = state;
    mp_context_append( node );
    return node;
}

void mp_context_remove( mp_context_node_t* node ){
    mp_context_node_t* predecessor = mp_context_predecessor(node);
    if( predecessor == NULL ){ return; }
    mp_context_node_t* successor = NULL;
    successor = node->next;
    predecessor->next = successor;
    mp_context_dynmem_free_all( node );
    MP_STATE_FREE(node->state);
    MP_STATE_FREE(node);
}

mp_context_node_t* mp_task_register( uint32_t tID, void* addtlargs ){
    mp_context_node_t* node = NULL;
    node = mp_context_by_tid( tID );
    if( node != NULL ){ return NULL; } // can't register the same task ID
    node = mp_context_append_new();
    if( node == NULL ){ return NULL; } // no heap
    node->id = tID;
    node->args.input_kind = 0;
    node->args.source = NULL;
    node->args.addtl = addtlargs;
    node->status = 0;
    return node;
}

void mp_task_remove( uint32_t tID ){
    mp_context_node_t* node = NULL;
    node = mp_context_by_tid( tID );
    if( node == mp_context_head){ return; }
    if( node == NULL ){ return; }
    mp_context_remove( node );
}

void mp_task_switched_in( uint32_t tID ){
    // todo: we need to handle when the switched-in task is a thread running underneath one of our contexts. (i.e. in the threadctrl->thread LL)
    mp_current_tIDs[MICROPY_GET_CORE_INDEX] = tID;
    mp_context_node_t* node = mp_context_by_tid( tID );
    if(node == NULL){ return; }
    mp_context_switch(node);
}


// context dynamic memory iterators
mp_context_dynmem_iter_t mp_dynmem_iter_first( mp_context_dynmem_iter_t head ){ return head; }
bool mp_dynmem_iter_done( mp_context_dynmem_iter_t iter ){ return (iter == NULL); }
mp_context_dynmem_iter_t mp_dynmem_iter_next( mp_context_dynmem_iter_t iter ){ return (MP_DYNMEM_PTR_FROM_ITER(iter)->next); }
void mp_dynmem_foreach(mp_context_dynmem_iter_t head, void (*f)(mp_context_dynmem_iter_t iter, void*), void* args){
    mp_context_dynmem_iter_t iter = NULL;
    for( iter = mp_dynmem_iter_first(head); !mp_dynmem_iter_done(iter); iter = mp_dynmem_iter_next(iter) ){ 
        f(iter, args); 
    }
}

// context dynamic memory linked list manipulation
mp_context_dynmem_node_t* mp_new_dynmem_node( void ){
    mp_context_dynmem_node_t* node = NULL;
    node = (mp_context_dynmem_node_t*)MP_STATE_MALLOC(1*sizeof(mp_context_dynmem_node_t));
    if(node == NULL){ return node; }
    memset((void*)node, 0x00, sizeof(mp_context_dynmem_node_t));
    return node;
}

mp_context_dynmem_node_t* mp_dynmem_predecessor( mp_context_dynmem_node_t* successor, mp_context_node_t* context ){
    if( context == NULL ){ return NULL; }
    mp_context_dynmem_iter_t iter = NULL;
    for( iter = mp_dynmem_iter_first(MP_ITER_FROM_DYNMEM_PTR(context->memhead)); !mp_dynmem_iter_done(iter); iter = mp_dynmem_iter_next(iter) ){
        if( MP_DYNMEM_PTR_FROM_ITER(iter)->next == successor ){ break; }
    }
    return MP_DYNMEM_PTR_FROM_ITER(iter);
}

mp_context_dynmem_node_t* mp_dynmem_tail( mp_context_node_t* context ){
    return mp_dynmem_predecessor( NULL, context );
}

void mp_dynmem_append( mp_context_dynmem_node_t* node, mp_context_node_t* context ){
    if( node == NULL ){ return; }
    if( context == NULL ){ return; }
    if( node->context != NULL ){ return; }

    node->next = NULL;
    if( context->memhead == NULL ){
        context->memhead = node;
    }else{
        mp_context_dynmem_node_t* tail = mp_dynmem_tail( context );
        if(tail == NULL){ return; }
        tail->next = node;
    }

    node->context = context;
}

void mp_dynmem_remove( mp_context_dynmem_node_t* node, mp_context_node_t* context ){
    if( node == NULL ){ return; }
    if( context == NULL ){ return; }

    if( context->memhead == node ){
        context->memhead = node->next;
    }else{
        mp_context_dynmem_node_t* predecessor = mp_dynmem_predecessor( node, context );
        if( predecessor == NULL ){ return; }
        predecessor->next = node->next;
    }
    if( node->mem != NULL ){
        MP_STATE_FREE(node->mem);
    }
    MP_STATE_FREE(node);
}

mp_context_dynmem_node_t* mp_dynmem_node_by_mem( void* mem, mp_context_node_t* context ){
    if( mem == NULL ){ return NULL; }
    if( context == NULL ){ return NULL; }
    mp_context_dynmem_iter_t iter = NULL;
    for( iter = mp_dynmem_iter_first(MP_ITER_FROM_DYNMEM_PTR(context->memhead)); !mp_dynmem_iter_done(iter); iter = mp_dynmem_iter_next(iter) ){
        if( MP_DYNMEM_PTR_FROM_ITER(iter)->mem == mem ){ break; }
    }
    return MP_DYNMEM_PTR_FROM_ITER(iter);
}

void* mp_context_dynmem_alloc( size_t size, mp_context_node_t* context ){
    if( context == NULL){ return NULL;}

    mp_context_dynmem_node_t* node = mp_new_dynmem_node();
    if( node == NULL ){ return NULL; } // no heap

    void* mem = MP_STATE_MALLOC(size);
    if( mem == NULL){
        MP_STATE_FREE(node);
        return NULL;
    }
    node->mem = mem;
    node->size = size;
    node->next = NULL;
    mp_dynmem_append( node, context );
    return mem;
}

int8_t mp_context_dynmem_free( void* mem, mp_context_node_t* context ){
    if( context == NULL){ return -1; }
    mp_context_dynmem_node_t* node = mp_dynmem_node_by_mem( mem, context );
    if( node == NULL ){ return -1; }
    if( node->mem != mem ){ return -1; }
    mp_dynmem_remove( node, context );
    return 0;
}

int8_t mp_context_dynmem_free_all( mp_context_node_t* context ){
    if( context == NULL){ return -1; }
    if( context->memhead == NULL){ return -1; }
    while( context->memhead != NULL ){
        if( context->memhead == NULL){ return -1; }
        mp_dynmem_remove( context->memhead, context );
    }
    return 0;
}

void* mp_task_alloc( size_t size, uint32_t tID ){
    mp_context_node_t* context = mp_context_by_tid( tID );
    if( context == NULL){ return NULL;}
    return mp_context_dynmem_alloc( size, context );
}

int8_t mp_task_free( void* mem, uint32_t tID ){
    mp_context_node_t* context = mp_context_by_tid( tID );
    if( context == NULL){ return -1; }
    return mp_context_dynmem_free( mem, context );
}

int8_t mp_task_free_all( uint32_t tID ){
    mp_context_node_t* context = mp_context_by_tid( tID );
    if( context == NULL){ return -1; }
    return mp_context_dynmem_free_all( context );
}


// globals
mp_state_ctx_t _hidden_mp_state_ctx;

mp_context_node_t mp_default_context = {
    .id = 0,
    .status = 0,
    .state = &_hidden_mp_state_ctx,
    .args = {   .input_kind = 0,
                .source = NULL,
                .addtl = NULL },
    .threadctrl = NULL,
    .memhead = NULL,
    .next = NULL,
};

mp_context_node_t  mp_active_context_mirrors[MICROPY_NUM_CORES];
mp_context_node_t* mp_active_contexts[MICROPY_NUM_CORES];
mp_context_node_t* mp_context_head = &mp_default_context;
volatile uint32_t mp_current_tIDs[MICROPY_NUM_CORES];

