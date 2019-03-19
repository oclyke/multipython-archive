


// todo: one day we can replace this file with a port-specific file that is designed to allocate / deallocate heaps for multipython tasks. 
// the default allocation can be a fixed-size malloc. 
// users would use the port file to override their desired allocation (e.g. for esp32 it would be with the SPIRAM)

#include "mpstate_spiram.h"
#include "esp_heap_caps.h"

#define MP_STATE_SPIRAM_MALLOC_HEAP_CAPS(size, caps) (heap_caps_malloc( size, caps ))

mp_context_dynmem_node_t* mp_new_dynmem_node_heap_caps( uint32_t caps ){
    mp_context_dynmem_node_t* node = NULL;

    // printf("about to alslocate the node with caps = %d\n", (uint32_t)caps );

    node = (mp_context_dynmem_node_t*)MP_STATE_SPIRAM_MALLOC_HEAP_CAPS( sizeof(mp_context_dynmem_node_t), caps );
    if(node == NULL){
        // printf("failed\n" );
        return node; 
    }
    memset((void*)node, 0x00, sizeof(mp_context_dynmem_node_t));
    return node;
}

void* mp_context_dynmem_alloc_heap_caps( size_t size, mp_context_node_t* context, uint32_t caps ){
    if( context == NULL){ return NULL;}

    mp_context_dynmem_node_t* node = mp_new_dynmem_node_heap_caps(caps);
    if( node == NULL ){ return NULL; } // no heap

    // printf("about to allocate the heap with caps = %d\n", (uint32_t)caps );

    void* mem = MP_STATE_SPIRAM_MALLOC_HEAP_CAPS(size, caps);
    if( mem == NULL){

        // printf("failed\n" );

        MP_STATE_FREE(node);
        return NULL;
    }
    node->mem = mem;
    node->size = size;
    node->next = NULL;
    mp_dynmem_append( node, context );
    return mem;
}

void* mp_task_alloc_heap_caps( size_t size, uint32_t tID, uint32_t caps ){
    mp_context_node_t* context = mp_context_by_tid( tID );
    // printf("got context: 0x%X\n", (uint32_t)context );
    if( context == NULL){ return NULL;}
    return mp_context_dynmem_alloc_heap_caps( size, context, caps );
}