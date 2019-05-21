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
#ifndef _MODADDRESSABLE_LAYER_H_
#define _MODADDRESSABLE_LAYER_H_

#include "modaddressable.h"

#define MODADD_LAYER_MALLOC(size)   (malloc(size))
#define MODADD_LAYER_FREE(ptr)      (free(ptr))

extern const mp_obj_type_t addressable_layerObj_type;

// this is the actual C-structure for our new object
typedef struct _addressable_layer_obj_t {
    mp_obj_base_t                   base;       // base represents some basic information, like type
    mp_obj_t                        fixture;    // the fixture that this layer is associated with
    volatile modadd_operations_e    op;         // defines how the layer is combined with the one before it
    uint8_t*                        data;       // the data for the layer
}addressable_layer_obj_t;

IRAM_ATTR void addressable_layer_compose(void* arg); // arg should be a pointer to the output controller

typedef modadd_layer_node_t* modadd_layer_iter_t;
modadd_layer_iter_t modadd_layer_iter_first( modadd_layer_iter_t head );
bool modadd_layer_iter_done( modadd_layer_iter_t iter );
modadd_layer_iter_t modadd_layer_iter_next( modadd_layer_iter_t iter );
void modadd_layer_foreach(modadd_layer_iter_t head, void (*f)(modadd_layer_iter_t iter, void*), void* args);

#define MODADD_LAYER_PTR_FROM_ITER(iter) ((modadd_layer_node_t*)iter)
#define MODADD_ITER_FROM_LAYER_PTR(ptr) ((modadd_layer_iter_t)ptr)

modadd_layer_node_t* modadd_new_layer_node( void );
modadd_layer_node_t* modadd_layer_predecessor( modadd_layer_node_t* successor, modadd_layer_node_t* base );
modadd_layer_node_t* modadd_layer_tail( modadd_layer_node_t* base );
void modadd_layer_append( modadd_layer_node_t* node, modadd_layer_node_t* base );
modadd_layer_node_t* modadd_layer_append_new( modadd_layer_node_t* base );
void modadd_layer_remove( modadd_layer_node_t* node, modadd_layer_node_t* base );

mp_obj_t addressable_layer_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args );

#endif // _MODADDRESSABLE_LAYER_H_