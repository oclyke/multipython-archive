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

#include "modaddressable_fixture.h"



////////////////////////////////////////////////////////////////////////////
/* MicroPython Fixture Class                                              */
////////////////////////////////////////////////////////////////////////////
STATIC mp_obj_t addressable_fixture_set(mp_obj_t self_in, mp_obj_t index, mp_obj_t rgb);
MP_DEFINE_CONST_FUN_OBJ_3(addressable_fixture_set_obj, addressable_fixture_set);

STATIC void addressable_fixture_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind );
mp_obj_t addressable_fixture_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args );


STATIC const mp_rom_map_elem_t addressable_fixture_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_set), MP_ROM_PTR(&addressable_fixture_set_obj) },
 };
STATIC MP_DEFINE_CONST_DICT(addressable_fixture_locals_dict, addressable_fixture_locals_dict_table);


// define the fixture class-object
const mp_obj_type_t addressable_fixtureObj_type = {
    { &mp_type_type },                              // "inherit" the type "type"
    .name = MP_QSTR_fixtureObj,                     // give it a name
    .print = addressable_fixture_print,             // give it a print-function
    .make_new = addressable_fixture_make_new,       // give it a constructor
    .locals_dict = (mp_obj_dict_t*)&addressable_fixture_locals_dict, // and the global members
};


addressable_fixture_obj_t mach1_stat_fixture_obj = {
    .base = {
        .type = &addressable_fixtureObj_type,
    },
    // .info = modadd_controllers[MACH1_CONTROLLER_STAT]->fixture_ctrl.head
};



mp_obj_t addressable_fixture_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args ) {
    enum { ARG_leds, ARG_protocol, ARG_name, ARG_trans, ARG_rot, ARG_template, ARG_get_stat_fixture };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_leds,     MP_ARG_INT,                     {.u_int = 0} },
        { MP_QSTR_protocol, MP_ARG_KW_ONLY | MP_ARG_INT,    {.u_int = MODADD_PROTOCOL_UNKNOWN } },
        { MP_QSTR_name,     MP_ARG_KW_ONLY | MP_ARG_OBJ,    {.u_obj = mp_const_none} },
        { MP_QSTR_trans,    MP_ARG_KW_ONLY | MP_ARG_OBJ,    {.u_obj = mp_const_none} },
        { MP_QSTR_rot,      MP_ARG_KW_ONLY | MP_ARG_OBJ,    {.u_obj = mp_const_none} },
        { MP_QSTR_template, MP_ARG_KW_ONLY | MP_ARG_OBJ,    {.u_obj = mp_const_none} },
        { MP_QSTR_get_stat_fixture, MP_ARG_KW_ONLY | MP_ARG_BOOL,    {.u_bool = false} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    
    // // this checks the number of arguments (min 1, max 1);
    // // on error -> raise python exception
    // mp_arg_check_num(n_args, n_kw, 1, 1, true);

    addressable_fixture_obj_t *self;
    if( args[ARG_get_stat_fixture].u_bool == true ){
        // we want the stat fixture, so get that
        mach1_stat_fixture_obj.info = *(modadd_controllers[MACH1_CONTROLLER_STAT]->fixture_ctrl.head);
        self = &mach1_stat_fixture_obj;
    }else{
        // create a new object of our C-struct type
        self = m_new_obj(addressable_fixture_obj_t);
        self->base.type = &addressable_fixtureObj_type; // give it a type
        memset((void*)&(self->info), 0x00, sizeof(modadd_fixture_t));

        if( args[ARG_template].u_obj == mp_const_none ){
            if(args[ARG_leds].u_int == 0){
                mp_raise_ValueError("Cannot make a fixture with 0 leds\n");
                return mp_const_none;
            }

            self->info.leds = args[ARG_leds].u_int;
            self->info.protocol = args[ARG_protocol].u_int;
            // printf("protocol is 0x%X\n", mp_obj_get_int_truncated(args[ARG_protocol].u_int));

            if( mp_obj_is_str(args[ARG_name].u_obj) ){
                // printf("name is '%s'\n", mp_obj_str_get_str(args[ARG_name].u_obj));
                self->info.name = (char*)mp_obj_str_get_str(args[ARG_name].u_obj);
            }/*else{
                // printf("no name supplied\n");
            }*/

            // todo: handle rotation and translation arguments

            // if( mp_obj_is_type(args[ARG_trans].u_obj, &mp_type_list) ){
            //  create new arrays of led locations
            //}else{
            //  no other types allowed for the trans input
            //}

            // if( mp_obj_is_type(args[ARG_rot].u_obj, &mp_type_list) ){
            //  create new arrays of led rotations
            //}else{
            //  no other types allowed for the trans input
            //}

            // todo: reconsider storing rotation / translation data on the ESP32... maybe OK just to use it on the phone?

        }else{
            if( mp_obj_is_type(args[ARG_template].u_obj, &addressable_fixtureObj_type) ){
                addressable_fixture_obj_t template = *((addressable_fixture_obj_t*)args[ARG_template].u_obj); 
                self->info = template.info;

                if( mp_obj_is_str(args[ARG_name].u_obj) ){
                    printf("name is '%s'\n", mp_obj_str_get_str(args[ARG_name].u_obj));
                    self->info.name = (char*)mp_obj_str_get_str(args[ARG_name].u_obj);
                }

            }else{
                printf("Error: template fixture must be of fixture type\n");
                return mp_const_none;
            }
        }
    }

    
    return MP_OBJ_FROM_PTR(self);
}

STATIC void addressable_fixture_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind ) {
    // get a ptr to the C-struct of the object
    addressable_fixture_obj_t *self = MP_OBJ_TO_PTR(self_in);
    // print the number
    printf ("Fixture class object with #leds = %d\n", self->info.leds);
}

STATIC mp_obj_t addressable_fixture_set(mp_obj_t self_in, mp_obj_t index_obj, mp_obj_t colors) {
    addressable_fixture_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if( self->info.protocol >= MODADD_PROTOCOLS_NUM ){
        mp_raise_ValueError("Fixture protocol unknown, cannot set data\n");
        return mp_const_none;
    }

    mp_int_t index;
    mp_obj_get_int_maybe( index_obj, &index );
    if( index >= self->info.leds ){ 
        mp_raise_ValueError("Index exceeds fixture LEDs\n"); 
        return mp_const_none;
    }

    if( mp_obj_is_type( colors, &mp_type_list) ){
        size_t colors_len;
        mp_obj_t* colors_items;
        mp_obj_list_get(colors, &colors_len, &colors_items);

        for(size_t color_ind = 0; color_ind < colors_len; color_ind++){
            if( mp_obj_is_type( colors, &mp_type_list) ){
                size_t color_len;
                mp_obj_t* color_items;
                mp_obj_list_get(colors, &color_len, &color_items);

                const modadd_protocol_t* protocol = modadd_protocols[self->info.protocol];

                uint8_t     bpl = protocol->bpl;
                uint8_t*    base = self->info.data;
                if( ( base == NULL ) ){ mp_raise_ValueError("There is no memory for this fixture - try adding the fixture to an output string\n"); }
                if( color_len < bpl ){
                    mp_raise_ValueError("Too few color elements for this protocol\n");
                    return mp_const_none;
                }
                for( uint8_t led_ind = 0; led_ind < bpl; led_ind++ ){
                    mp_int_t val;
                    mp_obj_get_int_maybe( (color_items[protocol->indices[led_ind]]), &val );
                    *(base + (color_ind*bpl) + led_ind) = (uint8_t)val;
                    *(base + (color_ind*bpl) + led_ind) |= protocol->or_mask[led_ind];
                }

            }else{
                mp_raise_TypeError("Elements of 'colors' should be lists of integers\n");
            }
        }
    }else{
        mp_raise_TypeError("color should be a list of lists of integers\n");
        return mp_const_none;
    }

    return mp_const_none;
}

















// fixtures
modadd_fixture_iter_t modadd_fixture_iter_first( modadd_fixture_iter_t head ){ return head; }
bool modadd_fixture_iter_done( modadd_fixture_iter_t iter ){ return (iter == NULL); }
modadd_fixture_iter_t modadd_fixture_iter_next( modadd_fixture_iter_t iter ){ return (MODADD_FIXTURE_PTR_FROM_ITER(iter)->next); }
void modadd_fixture_foreach(modadd_fixture_iter_t head, void (*f)(modadd_fixture_iter_t iter, void*), void* args){
    modadd_fixture_iter_t iter = NULL;
    for( iter = modadd_fixture_iter_first(head); !modadd_fixture_iter_done(iter); iter = modadd_fixture_iter_next(iter) ){ 
        f(iter, args); 
    }
}

// fixture linked list manipulation
modadd_fixture_t* modadd_new_fixture_node( void ){
    modadd_fixture_t* node = NULL;
    node = (modadd_fixture_t*)MODADD_MALLOC(1*sizeof(modadd_fixture_t));
    if(node == NULL){ return node; }
    memset((void*)node, 0x00, sizeof(modadd_fixture_t));
    return node;
}

modadd_fixture_t* modadd_fixture_predecessor( modadd_fixture_t* successor, modadd_ctrl_t* ctrl ){
    if( ctrl == NULL ){ return NULL; }
    if( successor == ctrl->fixture_ctrl.head ){ return NULL; }
    modadd_fixture_iter_t iter = NULL;
    for( iter = modadd_fixture_iter_first(MODADD_ITER_FROM_FIXTURE_PTR(ctrl->fixture_ctrl.head)); !modadd_fixture_iter_done(iter); iter = modadd_fixture_iter_next(iter) ){
        if( MODADD_FIXTURE_PTR_FROM_ITER(iter)->next == successor ){ break; }
    }
    return MODADD_FIXTURE_PTR_FROM_ITER(iter);
}

modadd_fixture_t* modadd_fixture_tail( modadd_ctrl_t* ctrl ){
    return modadd_fixture_predecessor( NULL, ctrl );
}

modadd_status_e modadd_fixture_append( modadd_fixture_t* node, modadd_ctrl_t* ctrl ){
    if( node == NULL ){ return MODADD_STAT_ERR; }
    if( ctrl == NULL ){ return MODADD_STAT_ERR; }

    node->next = NULL;
    if( ctrl->fixture_ctrl.head == NULL ){
        ctrl->fixture_ctrl.head = node;
    }else{
        modadd_fixture_t* tail = modadd_fixture_tail( ctrl );
        if(tail == NULL){ return MODADD_STAT_ERR; }
        tail->next = node;
    }
    // Removed recomputation from the append function to avoid memory fragmentation. Need to call manually when all desired fixtures are added
    return MODADD_STAT_OK;
}

modadd_fixture_t* modadd_fixture_append_new( modadd_ctrl_t* ctrl ){
    modadd_fixture_t* node = NULL;
    node = modadd_new_fixture_node();
    if( node == NULL ){ return NULL; } // no heap
    modadd_fixture_append( node, ctrl );
    return node;
}

modadd_status_e modadd_fixture_remove( modadd_fixture_t* node, modadd_ctrl_t* ctrl ){
    modadd_fixture_t* predecessor = modadd_fixture_predecessor(node, ctrl);
    if( predecessor == NULL ){ return MODADD_STAT_ERR; }
    modadd_fixture_t* successor = NULL;
    successor = node->next;
    predecessor->next = successor;
    MODADD_FREE(node);
    modadd_ctrl_recompute_fixtures( ctrl ); // ok to recompute after a removal, b/c memory requirements will not increase
    return MODADD_STAT_OK;
}







