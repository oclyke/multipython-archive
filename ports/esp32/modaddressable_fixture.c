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
// helper functions (not visible to users)
mp_obj_t addressable_fixture_get_dict( mp_obj_t fixture_obj, mp_int_t position ) {
    addressable_fixture_obj_t* fixture = MP_OBJ_TO_PTR(fixture_obj);

    const uint8_t numel = 8;
    mp_obj_t fixture_dict = mp_obj_new_dict(numel);

    const char* str_key_pos = "pos";
    const char* str_key_name = "name";
    const char* str_key_id = "id";
    const char* str_key_protocol = "protocol";
    const char* str_key_leds = "leds";
    const char* str_key_ctrl = "ctrl";
    const char* str_key_data = "data";
    // const char* str_key_next = "next";

    mp_obj_t key_pos = mp_obj_new_str_via_qstr(str_key_pos, strlen(str_key_pos));
    mp_obj_t key_name = mp_obj_new_str_via_qstr(str_key_name, strlen(str_key_name));
    mp_obj_t key_id = mp_obj_new_str_via_qstr(str_key_id, strlen(str_key_id));
    mp_obj_t key_protocol = mp_obj_new_str_via_qstr(str_key_protocol, strlen(str_key_protocol));
    mp_obj_t key_leds = mp_obj_new_str_via_qstr(str_key_leds, strlen(str_key_leds));
    mp_obj_t key_ctrl = mp_obj_new_str_via_qstr(str_key_ctrl, strlen(str_key_ctrl));
    mp_obj_t key_data = mp_obj_new_str_via_qstr(str_key_data, strlen(str_key_data));
    // mp_obj_t key_next = mp_obj_new_str_via_qstr(str_key_next, strlen(str_key_next));

    mp_obj_t pos;
    if(position < 0){ pos = mp_const_none; }
    else{ pos = mp_obj_new_int(position); }
    mp_obj_t name;
    if( fixture->name == NULL ){ name = mp_const_none; }
    else{ name = mp_obj_new_str_via_qstr( fixture->name, strlen(fixture->name) ); }
    mp_obj_t id = mp_obj_new_int( (mp_int_t)fixture->id );
    mp_obj_t protocol = mp_obj_new_int( (mp_int_t)fixture->protocol );
    mp_obj_t leds = mp_obj_new_int( (mp_int_t)fixture->leds );
    mp_obj_t ctrl = mp_obj_new_int( (mp_int_t)fixture->ctrl );
    mp_obj_t data = mp_obj_new_int( (mp_int_t)fixture->data );
    // mp_obj_t next = mp_obj_new_int( (mp_int_t)fixture->next );

    mp_obj_dict_store( fixture_dict,    key_pos,        pos         );
    mp_obj_dict_store( fixture_dict,    key_name,       name        );
    mp_obj_dict_store( fixture_dict,    key_id,         id          );
    mp_obj_dict_store( fixture_dict,    key_protocol,   protocol    );
    mp_obj_dict_store( fixture_dict,    key_leds,       leds        );
    mp_obj_dict_store( fixture_dict,    key_ctrl,       ctrl        );
    mp_obj_dict_store( fixture_dict,    key_data,       data        );
    // mp_obj_dict_store( fixture_dict,    key_next,       next        );

    return fixture_dict;
}





STATIC mp_obj_t addressable_fixture_add_layer(mp_obj_t self_in);
MP_DEFINE_CONST_FUN_OBJ_1(addressable_fixture_add_layer_obj, addressable_fixture_add_layer);

STATIC mp_obj_t addressable_fixture_layers(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(addressable_fixture_layers_obj, 0, addressable_fixture_layers);

STATIC mp_obj_t addressable_fixture_set(mp_obj_t self_in, mp_obj_t index, mp_obj_t rgb);
MP_DEFINE_CONST_FUN_OBJ_3(addressable_fixture_set_obj, addressable_fixture_set);

mp_obj_t addressable_fixture_artnet(mp_obj_t self_in, mp_obj_t bright );
MP_DEFINE_CONST_FUN_OBJ_2(addressable_fixture_artnet_obj, addressable_fixture_artnet);

STATIC void addressable_fixture_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind );
// mp_obj_t addressable_fixture_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args );


STATIC const mp_rom_map_elem_t addressable_fixture_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_add_layer), MP_ROM_PTR(&addressable_fixture_add_layer_obj) },
    { MP_ROM_QSTR(MP_QSTR_layers), MP_ROM_PTR(&addressable_fixture_layers_obj) },
    { MP_ROM_QSTR(MP_QSTR_set), MP_ROM_PTR(&addressable_fixture_set_obj) },
    { MP_ROM_QSTR(MP_QSTR_artnet), MP_ROM_PTR(&addressable_fixture_artnet_obj) },
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

// extern addressable_fixture_obj_t mach1_stat_fixture_obj;



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

    addressable_fixture_obj_t *self = NULL;
    if( args[ARG_get_stat_fixture].u_bool == true ){
        // // we want the stat fixture, so get that
        // addressable_fixture_obj_t* fix = (addressable_fixture_obj_t*)modadd_controllers[MACH1_CONTROLLER_STAT]->fixture_ctrl.head->fixture;
        // mach1_stat_fixture_obj.info = fix->info;
        // self = &mach1_stat_fixture_obj;

        // todo: fix this (maybe done with below?)

        return &machone_stat_fixture_obj;

    }else{
        // // dynamically allocate the modadd_fixture_t that this object will point to 
        // modadd_fixture_t* fix_info = (modadd_fixture_t*)MODADD_MALLOC(sizeof(modadd_fixture_t));
        // if( fix_info == NULL ){             // todo: !!! make sure there is a way to get rid of this memory when not needed !!! 
        //     mp_raise_OSError(MP_ENOMEM);
        //     return mp_const_none;
        // }
        // memset( (void*)fix_info, 0x00, sizeof(modadd_fixture_t) );

        // create a new object of our C-struct type
        self = m_new_obj(addressable_fixture_obj_t);
        memset( (void*)self, 0x00, sizeof(addressable_fixture_obj_t) );
        self->base.type = &addressable_fixtureObj_type; // give it a type

        if( args[ARG_template].u_obj == mp_const_none ){
            // if(args[ARG_leds].u_int == 0){
            //     mp_raise_ValueError("Cannot make a fixture with 0 leds\n");
            //     return mp_const_none;
            // }

            self->leds = args[ARG_leds].u_int;
            self->protocol = args[ARG_protocol].u_int;
            // printf("protocol is 0x%X\n", mp_obj_get_int_truncated(args[ARG_protocol].u_int));

            if( mp_obj_is_str(args[ARG_name].u_obj) ){
                // printf("name is '%s'\n", mp_obj_str_get_str(args[ARG_name].u_obj));
                self->name = (char*)mp_obj_str_get_str(args[ARG_name].u_obj);
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
                
                // todo: do I even want this template feature?
                printf("Error: templates not currently supported\n");
                return mp_const_none;
                
                // addressable_fixture_obj_t template = *((addressable_fixture_obj_t*)args[ARG_template].u_obj); 
                // self->info = template.info;

                // if( mp_obj_is_str(args[ARG_name].u_obj) ){
                //     printf("name is '%s'\n", mp_obj_str_get_str(args[ARG_name].u_obj));
                //     self->info->name = (char*)mp_obj_str_get_str(args[ARG_name].u_obj);
                // }

            }else{
                printf("Error: template fixture must be of fixture type\n");
                return mp_const_none;
            }
        }
    }

    return MP_OBJ_FROM_PTR(self);
}

extern void dict_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind);
STATIC void addressable_fixture_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind ) {
    // get a ptr to the C-struct of the object
    addressable_fixture_obj_t *self = MP_OBJ_TO_PTR(self_in);
    // print the number
    // printf ("Fixture class object with #leds = %d\n", self->info->leds);
    printf ("Fixture class object: ");
    dict_print(&mp_plat_print, addressable_fixture_get_dict( self_in, -1 ), kind);
}


STATIC mp_obj_t addressable_fixture_add_layer(mp_obj_t self_in){
    addressable_fixture_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_t layer = addressable_layer_make_new( &addressable_layerObj_type, 1, 0, &self_in );
    modadd_layer_node_t* node = NULL;
    if( self->layers == NULL ){ 
        node = modadd_new_layer_node(); 
        self->layers = node;                                    // if there are no layers then set layers to the new node
    }   
    else{ 
        node = modadd_layer_append_new( self->layers );         // if there are layers then append a new node
    }
    if(node == NULL){ return mp_const_none; }
    node->layer = layer;
    return layer;
}

STATIC mp_obj_t addressable_fixture_layers(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // get list of layers, or single layer if index provided
    enum { ARG_self_in, ARG_index };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_self_in,  MP_ARG_REQUIRED | MP_ARG_OBJ,   {.u_obj = mp_const_none} },
        { MP_QSTR_index,    MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if( !mp_obj_is_type( args[ARG_self_in].u_obj, &addressable_fixtureObj_type ) ){
        mp_raise_TypeError("the first argument 'self' must be a fixture class object\n");
    }
    addressable_fixture_obj_t *self = MP_OBJ_TO_PTR(args[ARG_self_in].u_obj);

    mp_obj_t layers_list = mp_obj_new_list( 0, NULL );
    modadd_layer_iter_t iter = NULL;

    if( !mp_obj_is_type( args[ARG_index].u_obj, &mp_type_NoneType ) ){
        if( mp_obj_is_int( args[ARG_index].u_obj ) ){
            uint32_t layer_index = 0;
            uint32_t desired_index = mp_obj_int_get_truncated( args[ARG_index].u_obj );
            for( iter = modadd_layer_iter_first(MODADD_ITER_FROM_LAYER_PTR( self->layers )); !modadd_layer_iter_done(iter); iter = modadd_layer_iter_next(iter) ){
                if( layer_index == desired_index ){
                    addressable_layer_obj_t* layer = MODADD_LAYER_PTR_FROM_ITER( iter )->layer;

                    // todo: does returning just this pointer work? are there lifespan problems? Do I need to make an alias of the layer as a new mp object?

                    // mp_obj_t layer = addressable_layer_make_new( &addressable_layerObj_type, 0, 0, NULL );
                    // addressable_fixture_obj_t* p_fix = MP_OBJ_TO_PTR(fix);
                    // p_fix->info = fixture;   // point to the same data
                    // // mp_obj_list_append(fixtures_list, addressable_fixture_get_dict( fix, fixture_index ) );
                    // //mp_obj_list_append(fixtures_list, fix );

                    return layer;
                }
                layer_index++;
            }
            return mp_const_none;
        }else if( mp_obj_is_type( args[ARG_index].u_obj, &mp_type_list ) ){
            size_t size;
            mp_obj_t* items;
            mp_obj_list_get( args[ARG_index].u_obj, &size, &items );

            for( size_t list_index = 0; list_index < size; list_index++ ){
                mp_obj_t element = items[list_index];
                if( mp_obj_is_int( element ) ){
                    uint32_t layer_index = 0;
                    uint32_t desired_index = mp_obj_int_get_truncated( element );
                    for( iter = modadd_layer_iter_first(MODADD_ITER_FROM_LAYER_PTR( self->layers )); !modadd_layer_iter_done(iter); iter = modadd_layer_iter_next(iter) ){
                        if( layer_index == desired_index ){
                            addressable_layer_obj_t* layer = MODADD_LAYER_PTR_FROM_ITER( iter )->layer; // todo: still not sure if I need to make this pointer into a new mp object for lifespan considerations...?
                            mp_obj_list_append(layers_list, layer );
                        }
                        layer_index++;
                    }
                }
            }
        }else{
            mp_raise_TypeError("index must be an integer or list of integers\n");
            return mp_const_none;
        }
    }else{
        uint32_t count = 0;
        for( iter = modadd_layer_iter_first(MODADD_ITER_FROM_LAYER_PTR( self->layers )); !modadd_layer_iter_done(iter); iter = modadd_layer_iter_next(iter) ){
            addressable_layer_obj_t* layer = MODADD_LAYER_PTR_FROM_ITER( iter )->layer;
            mp_obj_list_append(layers_list, layer );
            count++;
        }
    }
    return layers_list;
}

STATIC mp_obj_t addressable_fixture_set(mp_obj_t self_in, mp_obj_t start_index_obj, mp_obj_t colors) {
    addressable_fixture_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if( self->protocol >= MODADD_PROTOCOLS_NUM ){
        mp_raise_ValueError("Fixture protocol unknown, cannot set data\n");
        return mp_const_none;
    }

    mp_int_t start_index;
    mp_obj_get_int_maybe( start_index_obj, &start_index );

    // printf("index = %d\n", index);

    if( start_index >= self->leds ){ 
        mp_raise_ValueError("Index exceeds fixture LEDs\n"); 
        return mp_const_none;
    }

    if( mp_obj_is_type( colors, &mp_type_list) ){
        size_t colors_len;
        mp_obj_t* colors_items;
        mp_obj_list_get(colors, &colors_len, &colors_items);

        // printf("the number of colors supplied is: %d\n", colors_len);
        // printf("the address of the first color is: 0x%X\n", (uint32_t)colors_items);

        const modadd_protocol_t* protocol = modadd_protocols[self->protocol];
        uint8_t     bpl = protocol->bpl;
        uint8_t*    base = self->data;

        // printf("base address for this fixture = 0x%X\n", (uint32_t)base );

        for(size_t color_ind = 0; color_ind < colors_len; color_ind++){
            if( mp_obj_is_type( colors_items[color_ind], &mp_type_list) ){
                size_t color_len;
                mp_obj_t* color_components;
                mp_obj_list_get(colors_items[color_ind], &color_len, &color_components);

                

                uint32_t index = color_ind + start_index;
                if( index >= (self->leds) ){ break; } // don't write into other fixture's memory!

                if( ( base == NULL ) ){ mp_raise_ValueError("There is no memory for this fixture - try adding the fixture to an output string\n"); }
                if( color_len < bpl ){
                    mp_raise_ValueError("Too few color elements for this protocol\n");
                    return mp_const_none;
                }

                for( uint8_t component_ind = 0; component_ind < bpl; component_ind++ ){
                    mp_int_t val;
                    mp_obj_get_int_maybe( (color_components[protocol->indices[component_ind]]), &val );

                    uint8_t* location = base + ((index)*bpl) + component_ind;

                    *(location) = (uint8_t)val;                         // set value
                    *(location) |= protocol->or_mask[component_ind];    // set mask

                    // printf("component_ind = %d, final value = %d, location = 0x%X\n", component_ind, (uint8_t)(*location), (uint32_t)location );
                }
                // printf("\n");

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

mp_obj_t addressable_fixture_artnet(mp_obj_t self_in, mp_obj_t bright ){
    addressable_fixture_obj_t *self = MP_OBJ_TO_PTR(self_in);

    const modadd_protocol_t* protocol = modadd_protocols[self->protocol];
    uint8_t     bpl = protocol->bpl;
    
    uint32_t fixture_leds = self->leds;
    uint8_t*    base = self->data;
    uint32_t artnet_ind = 0;
    uint8_t color_arry[8];

    // map values need to be set so that: 
    //  color_array[0] = red
    //  color_array[0] = green
    //  color_array[0] = blue
    //  color_array[0] = alpha

    const uint8_t map[8] = {0,1,2,3,4,5,6,7};

    uint8_t global = 0;
    const uint8_t def_brightness = 15;

    if(!mp_obj_is_int(bright)){
        global = def_brightness;
    }else{
        global = mp_obj_int_get_truncated(bright);
    }


    for(uint32_t led_ind = 0; led_ind < fixture_leds; led_ind++){

        artnet_packet.Data[artnet_ind];

        // set component array in the rgba format...

        // color_arry[0] = artnet_packet.Data[ artnet_ind + map[0] ];
        // color_arry[1] = artnet_packet.Data[ artnet_ind + map[1] ];
        // color_arry[2] = artnet_packet.Data[ artnet_ind + map[2] ];
        // color_arry[3] = artnet_packet.Data[ artnet_ind + map[3] ];
        
        // color_arry[3] = global;
        // color_arry[4] = artnet_packet.Data[ artnet_ind + map[4] ];
        // color_arry[5] = artnet_packet.Data[ artnet_ind + map[5] ];
        // color_arry[6] = artnet_packet.Data[ artnet_ind + map[6] ];
        // color_arry[7] = artnet_packet.Data[ artnet_ind + map[7] ];


        // color_arry[0] = artnet_packet.Data[(artnet_ind*3)+2];
        // color_arry[1] = artnet_packet.Data[(artnet_ind*3)+1];
        // color_arry[2] = artnet_packet.Data[(artnet_ind*3)+0];
        // color_arry[3] = global;


        // for( uint8_t component_ind = 0; component_ind < bpl; component_ind++ ){
        //     uint8_t* location = base + ((led_ind)*bpl) + component_ind;
        //     *(location) = color_arry[protocol->indices[component_ind]]; // set value
        //     *(location) |= protocol->or_mask[component_ind];            // set mask
        // }

        self->data[led_ind*4 + 0] = 0xE0 | global;                             // todo: handle universes and follow the protocol of the given fixture
        self->data[led_ind*4 + 1] = artnet_packet.Data[(artnet_ind*3)+0];
        self->data[led_ind*4 + 2] = artnet_packet.Data[(artnet_ind*3)+1];
        self->data[led_ind*4 + 3] = artnet_packet.Data[(artnet_ind*3)+2];

        artnet_ind++;

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

modadd_ctrl_t* modadd_fixture_get_ctrl( modadd_fixture_node_t* base ){
    addressable_fixture_obj_t* fixture = (addressable_fixture_obj_t*)base->fixture;
    return fixture->ctrl;
}

// fixture linked list manipulation
modadd_fixture_node_t* modadd_new_fixture_node( void ){
    modadd_fixture_node_t* node = NULL;
    node = (modadd_fixture_node_t*)MODADD_MALLOC(1*sizeof(modadd_fixture_node_t));
    if(node == NULL){ return node; }
    memset((void*)node, 0x00, sizeof(modadd_fixture_node_t));
    return node;
}

modadd_fixture_node_t* modadd_fixture_predecessor( modadd_fixture_node_t* successor, modadd_fixture_node_t* base ){
    if( base == NULL ){ return NULL; }
    if( successor == base ){ return NULL; }
    modadd_fixture_iter_t iter = NULL;
    for( iter = modadd_fixture_iter_first(MODADD_ITER_FROM_FIXTURE_PTR(base)); !modadd_fixture_iter_done(iter); iter = modadd_fixture_iter_next(iter) ){
        if( MODADD_FIXTURE_PTR_FROM_ITER(iter)->next == successor ){ break; }
    }
    return MODADD_FIXTURE_PTR_FROM_ITER(iter);
}

modadd_fixture_node_t* modadd_fixture_tail( modadd_fixture_node_t* base ){
    return modadd_fixture_predecessor( NULL, base );
}

modadd_status_e modadd_fixture_append_node( modadd_fixture_node_t* node, modadd_fixture_node_t* base ){
    if( node == NULL ){ return MODADD_STAT_ERR; }
    if( base == NULL ){ return MODADD_STAT_ERR; }
    modadd_fixture_node_t* tail = modadd_fixture_tail( base );
    if(tail == NULL){ return MODADD_STAT_ERR; }
    tail->next = node;
    // Removed recomputation from the append function to avoid memory fragmentation. Need to call manually when all desired fixtures are added
    return MODADD_STAT_OK;
}

modadd_fixture_node_t* modadd_fixture_append_new_node( modadd_fixture_node_t* base ){
    modadd_fixture_node_t* node = NULL;
    node = modadd_new_fixture_node();
    if( node == NULL ){ return NULL; } // no heap
    modadd_fixture_append_node( node, base );
    return node;
}

modadd_status_e modadd_fixture_remove_node( modadd_fixture_node_t* node, modadd_fixture_node_t* base ){
    if( base == NULL ){ return MODADD_STAT_ERR; }
    modadd_fixture_node_t* predecessor = modadd_fixture_predecessor(node, base );
    if( predecessor == NULL ){ return MODADD_STAT_ERR; }
    modadd_fixture_node_t* successor = NULL;
    successor = node->next;
    predecessor->next = successor;
    MODADD_FREE(node->fixture);
    MODADD_FREE(node);
    modadd_ctrl_recompute_fixtures( modadd_fixture_get_ctrl( base ) ); // ok to recompute after a removal, b/c memory requirements will not increase
    return MODADD_STAT_OK;
}


modadd_status_e modadd_fixture_append( addressable_fixture_obj_t* fixture, modadd_ctrl_t* ctrl ){
    if( fixture == NULL ){ return MODADD_STAT_ERR; }
    if( ctrl == NULL ){ return MODADD_STAT_ERR; }

    modadd_fixture_node_t* node = NULL;
    if( ctrl->fixture_ctrl.head == NULL ){
        node = modadd_new_fixture_node();
        ctrl->fixture_ctrl.head = node;
    }else{
        node = modadd_fixture_append_new_node( ctrl->fixture_ctrl.head );
    }
    if( node == NULL ){ return MODADD_STAT_ERR; } // no heap
    node->fixture = fixture;
    return MODADD_STAT_OK;
}

modadd_status_e modadd_fixture_remove( addressable_fixture_obj_t* fixture, modadd_ctrl_t* ctrl ){
    modadd_fixture_iter_t iter = NULL;
    modadd_fixture_node_t* base = ctrl->fixture_ctrl.head;
    for( iter = modadd_fixture_iter_first(MODADD_ITER_FROM_FIXTURE_PTR(base)); !modadd_fixture_iter_done(iter); iter = modadd_fixture_iter_next(iter) ){
        if( MODADD_FIXTURE_PTR_FROM_ITER(iter)->fixture == fixture ){ break; }
    }
    modadd_fixture_node_t* node = MODADD_FIXTURE_PTR_FROM_ITER(iter);

    modadd_fixture_node_t* predecessor = modadd_fixture_predecessor(node, base);
    if( predecessor == NULL ){ return MODADD_STAT_ERR; }
    modadd_fixture_node_t* successor = NULL;
    successor = node->next;
    predecessor->next = successor;
    MODADD_FREE(node);
    modadd_ctrl_recompute_fixtures( modadd_fixture_get_ctrl(base) ); // ok to recompute after a removal, b/c memory requirements will not increase
    return MODADD_STAT_OK;
}





// modadd_fixture_copy_info( addressable_fixture_obj_t* dest, addressable_fixture_obj_t* source ){
//     dest->id = source->id;
//     dest->
// }





