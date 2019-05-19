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

#include "modaddressable_layer.h"

////////////////////////////////////////////////////////////////////////////
/* MicroPython Fixture Class                                              */
////////////////////////////////////////////////////////////////////////////
// STATIC mp_obj_t addressable_layer_set(mp_obj_t self_in, mp_obj_t index, mp_obj_t rgb);
// MP_DEFINE_CONST_FUN_OBJ_3(addressable_layer_set_obj, addressable_layer_set);

STATIC void addressable_layer_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind );

STATIC const mp_rom_map_elem_t addressable_layer_locals_dict_table[] = {
    // { MP_ROM_QSTR(MP_QSTR_set), MP_ROM_PTR(&addressable_layer_set_obj) },
    // { MP_ROM_QSTR(MP_QSTR_artnet), MP_ROM_PTR(&addressable_fixture_artnet_obj) },
 };
STATIC MP_DEFINE_CONST_DICT(addressable_layer_locals_dict, addressable_layer_locals_dict_table);


// define the layer class-object
const mp_obj_type_t addressable_layerObj_type = {
    { &mp_type_type },                              // "inherit" the type "type"
    .name = MP_QSTR_layerObj,                       // give it a name
    .print = addressable_layer_print,             // give it a print-function
    .make_new = addressable_layer_make_new,       // give it a constructor
    .locals_dict = (mp_obj_dict_t*)&addressable_layer_locals_dict, // and the global members
};


// addressable_fixture_obj_t mach1_stat_fixture_obj = {
//     .base = {
//         .type = &addressable_fixtureObj_type,
//     },
//     // .info = modadd_controllers[MACH1_CONTROLLER_STAT]->fixture_ctrl.head
// };



mp_obj_t addressable_layer_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args ) {
    enum { ARG_fixture };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_fixture,     MP_ARG_REQUIRED | MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    
    // this checks the number of arguments (min 1, max 1);
    // on error -> raise python exception
    mp_arg_check_num(n_args, n_kw, 1, 1, true);

    if( !mp_obj_is_type( args[ARG_fixture].u_obj, &addressable_fixtureObj_type ) ){
        mp_raise_ValueError("Layers expect a parent fixture as the argument\n");
        return mp_const_none;
    }
    addressable_fixture_obj_t* fixture = (addressable_fixture_obj_t*)MP_OBJ_TO_PTR(args[ARG_fixture].u_obj); // extract the fixture type
    const modadd_protocol_t*    protocol = modadd_protocols[fixture->info->protocol];
    uint8_t                     bpl = protocol->bpl;
    uint32_t len = fixture->info->leds * bpl; // determine the length of the layer data
    
    // Try to allocate enough memory for the data
    uint8_t* data = NULL;
    data = (uint8_t*)MODADD_LAYER_MALLOC(len*sizeof(uint8_t));
    if(data == NULL){
        mp_raise_OSError(MP_ENOMEM);
        return mp_const_none;
    }

    // create a new object of our C-struct type
    addressable_layer_obj_t *self;
    self = m_new_obj(addressable_layer_obj_t);
    self->base.type = &addressable_layerObj_type; // give it a type
    self->fixture = MP_OBJ_TO_PTR(args[ARG_fixture].u_obj);
    self->data = data;
    
    return MP_OBJ_FROM_PTR(self);
}

STATIC void addressable_layer_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind ) {
    // get a ptr to the C-struct of the object
    addressable_layer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    // print the number
    addressable_fixture_obj_t* fixture = (addressable_fixture_obj_t*)self->fixture;

    // printf ("Layer class object with data at address = %d\n", (uint32_t)self->data);
    printf ("Layer class object for fixture id = %d\n", (uint32_t)fixture->info->id );
}











// layer iterator functions
modadd_layer_iter_t modadd_layer_iter_first( modadd_layer_iter_t head ){ return head; }
bool modadd_layer_iter_done( modadd_layer_iter_t iter ){ return (iter == NULL); }
modadd_layer_iter_t modadd_layer_iter_next( modadd_layer_iter_t iter ){ return (MODADD_LAYER_PTR_FROM_ITER(iter)->next); }
void modadd_layer_foreach(modadd_layer_iter_t head, void (*f)(modadd_layer_iter_t iter, void*), void* args){
    modadd_layer_iter_t iter = NULL;
    for( iter = modadd_layer_iter_first(head); !modadd_layer_iter_done(iter); iter = modadd_layer_iter_next(iter) ){ 
        f(iter, args); 
    }
}

// layer linked list manipulation
modadd_layer_node_t* modadd_new_layer_node( void ){
    modadd_layer_node_t* node = NULL;
    node = (modadd_layer_node_t*)MODADD_LAYER_MALLOC(1*sizeof(modadd_layer_node_t));
    if(node == NULL){ return node; }
    memset((void*)node, 0x00, sizeof(modadd_layer_node_t));
    return node;
}

modadd_layer_node_t* modadd_layer_predecessor( modadd_layer_node_t* successor, modadd_layer_node_t* base ){
    modadd_layer_iter_t iter = NULL;
    for( iter = modadd_layer_iter_first(MODADD_ITER_FROM_LAYER_PTR(base)); !modadd_layer_iter_done(iter); iter = modadd_layer_iter_next(iter) ){
        if(MODADD_LAYER_PTR_FROM_ITER(iter)->next == successor){ break; }
    }
    return MODADD_LAYER_PTR_FROM_ITER(iter);
}

modadd_layer_node_t* modadd_layer_tail( modadd_layer_node_t* base ){
    return modadd_layer_predecessor(NULL, base);
}

void modadd_layer_append( modadd_layer_node_t* node, modadd_layer_node_t* base ){
    if(node == NULL){ return; }
    modadd_layer_node_t* tail = modadd_layer_tail(base);
    if(tail == NULL){ return; }
    tail->next = node;
    node->next = NULL;
}

modadd_layer_node_t* modadd_layer_append_new( modadd_layer_node_t* base ){
    modadd_layer_node_t* node = NULL;
    node = modadd_new_layer_node();
    if( node == NULL ){ return NULL; } // no heap
    modadd_layer_append( node, base );
    return node;
}

void modadd_layer_remove( modadd_layer_node_t* node, modadd_layer_node_t* base ){
    modadd_layer_node_t* predecessor = modadd_layer_predecessor(node, base);
    if( predecessor == NULL ){ return; }
    modadd_layer_node_t* successor = NULL;
    successor = node->next;
    predecessor->next = successor;
    MODADD_LAYER_FREE(node);
}