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

// http://www.cs.princeton.edu/courses/archive/fall00/cs426/papers/smith95a.pdf
#define MODADD_INT_MULT(a,b,t)          ((t) = (a)*(b)+ 0x80, ((((t)>>8)+(t))>>8))  // INT_MULT is the integer approximation of the floating point 'a*b' where inputs and outputs are on [0,255] (and 0->0.0, 255->1.0)
#define MODADD_INT_LERP(p,q,a,t)        ((p)+MODADD_INT_MULT(a,((q)-(p),t)))        // INT_LERP performs a linear interpolation between values p and q (both on [0,255]) by weight a (alson on [0,255]). The result is on [0,255]
#define MODADD_INT_PRELERP(p,q,a,t)     ((p)+(q)-MODADD_INT_MULT(a,p,t))            // INT_PRELERP is also a linear interpolation, however it is assumed that q has been premultiplied by a using INT_MULT. Therefore q <= a in any valid operation.

////////////////////////////////////////////////////////////////////////////
/* MicroPython Fixture Class                                              */
////////////////////////////////////////////////////////////////////////////

STATIC mp_obj_t addressable_layer_mode(mp_obj_t self_in, mp_obj_t mode);
MP_DEFINE_CONST_FUN_OBJ_2(addressable_layer_mode_obj, addressable_layer_mode);

STATIC mp_obj_t addressable_layer_set(mp_obj_t self_in, mp_obj_t start_index_obj, mp_obj_t colors);
MP_DEFINE_CONST_FUN_OBJ_3(addressable_layer_set_obj, addressable_layer_set);

STATIC mp_obj_t addressable_layer_add_artdmx_info(mp_obj_t self_in, mp_obj_t info_in);
MP_DEFINE_CONST_FUN_OBJ_2(addressable_layer_add_artdmx_info_obj, addressable_layer_add_artdmx_info);

STATIC void addressable_layer_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind );

STATIC const mp_rom_map_elem_t addressable_layer_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_mode), MP_ROM_PTR(&addressable_layer_mode_obj) },
    { MP_ROM_QSTR(MP_QSTR_set), MP_ROM_PTR(&addressable_layer_set_obj) },
    
    { MP_ROM_QSTR(MP_QSTR_add_artdmx_info), MP_ROM_PTR(&addressable_layer_add_artdmx_info_obj) },

    { MP_ROM_QSTR(MP_QSTR_SKIP), MP_ROM_INT(MODADD_OP_SKIP) },
    { MP_ROM_QSTR(MP_QSTR_SET), MP_ROM_INT(MODADD_OP_SET) },
    { MP_ROM_QSTR(MP_QSTR_OR), MP_ROM_INT(MODADD_OP_OR) },
    { MP_ROM_QSTR(MP_QSTR_AND), MP_ROM_INT(MODADD_OP_AND) },
    { MP_ROM_QSTR(MP_QSTR_XOR), MP_ROM_INT(MODADD_OP_XOR) },
    { MP_ROM_QSTR(MP_QSTR_MULT), MP_ROM_INT(MODADD_OP_MULT) },
    { MP_ROM_QSTR(MP_QSTR_DIV), MP_ROM_INT(MODADD_OP_DIV) },
    { MP_ROM_QSTR(MP_QSTR_ADD), MP_ROM_INT(MODADD_OP_ADD) },
    { MP_ROM_QSTR(MP_QSTR_SUB), MP_ROM_INT(MODADD_OP_SUB) },
    { MP_ROM_QSTR(MP_QSTR_COMP), MP_ROM_INT(MODADD_OP_COMP) }, // premultiplied alpha composite
    { MP_ROM_QSTR(MP_QSTR_MASK), MP_ROM_INT(MODADD_OP_MASK) }, // clears value if non-zero
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
    // const modadd_protocol_t*    protocol = modadd_protocols[fixture->protocol];
    // uint8_t                     bpl = protocol->bpl;
    // uint32_t len = fixture->leds * bpl; // determine the length of the layer data
    uint32_t len = fixture->leds * MODADD_BPL; // determine the length of the layer data - now using standard 4 bytes per pixel, bpl is only relevant in context of an output protocol
    
    // Try to allocate enough memory for the data
    uint8_t* data = NULL;
    data = (uint8_t*)MODADD_LAYER_MALLOC(len*sizeof(uint8_t));
    if(data == NULL){
        mp_raise_OSError(MP_ENOMEM);
        return mp_const_none;
    }
    memset(data, 0x00, len);

    // create a new object of our C-struct type
    addressable_layer_obj_t *self;
    self = m_new_obj(addressable_layer_obj_t);
    self->base.type = &addressable_layerObj_type; // give it a type
    self->fixture = MP_OBJ_TO_PTR(args[ARG_fixture].u_obj);
    self->data = data;
    self->op = MODADD_OP_COMP; // defaulting to compositing operations, with blank data
    
    return MP_OBJ_FROM_PTR(self);
}

STATIC void addressable_layer_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind ) {
    // get a ptr to the C-struct of the object
    addressable_layer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    // // print the number
    // addressable_fixture_obj_t* fixture = (addressable_fixture_obj_t*)self->fixture;

    // printf ("Layer class object with data at address = %d\n", (uint32_t)self->data);
    printf ("Layer class object with mode = %d\n", (uint32_t)self->op );
}


STATIC mp_obj_t addressable_layer_mode(mp_obj_t self_in, mp_obj_t mode){
    addressable_layer_obj_t* self = (addressable_layer_obj_t*)MP_OBJ_TO_PTR(self_in);
    if(! mp_obj_is_int( mode )){ return mp_const_none; }
    self->op = (modadd_operations_e)mp_obj_get_int(mode);
    return mp_const_none;
}

STATIC mp_obj_t addressable_layer_set(mp_obj_t self_in, mp_obj_t start_index_obj, mp_obj_t colors) {
    addressable_layer_obj_t *self = (addressable_layer_obj_t*)MP_OBJ_TO_PTR(self_in);
    addressable_fixture_obj_t* fixture = (addressable_fixture_obj_t*)self->fixture;

    mp_int_t start_index;
    mp_obj_get_int_maybe( start_index_obj, &start_index );

    if( start_index >= fixture->leds ){ 
        mp_raise_ValueError("Index exceeds fixture LEDs\n"); 
        return mp_const_none;
    }

    if( mp_obj_is_type( colors, &mp_type_list) ){
        size_t colors_len;
        mp_obj_t* colors_items;
        mp_obj_list_get(colors, &colors_len, &colors_items);

        // printf("the number of colors supplied is: %d\n", colors_len);
        // printf("the address of the first color is: 0x%X\n", (uint32_t)colors_items);

        // const modadd_protocol_t* protocol = modadd_protocols[fixture->protocol];
        // uint8_t     bpl = protocol->bpl;
        uint8_t*    base = self->data;

        // printf("base address for this fixture = 0x%X\n", (uint32_t)base );

        for(size_t color_ind = 0; color_ind < colors_len; color_ind++){
            if( mp_obj_is_type( colors_items[color_ind], &mp_type_list) ){
                size_t color_len;
                mp_obj_t* color_components;
                mp_obj_list_get(colors_items[color_ind], &color_len, &color_components);

                size_t index = color_ind + start_index;
                if( index >= (fixture->leds) ){ break; } // don't write into other fixture's memory!

                if( ( base == NULL ) ){ mp_raise_ValueError("There is no memory for this layer\n"); }
                if( color_len != MODADD_BPL ){
                    mp_raise_ValueError("Too few color elements for this protocol\n");
                    return mp_const_none;
                }

                for( uint8_t component_ind = 0; component_ind < MODADD_BPL; component_ind++ ){
                    mp_int_t val;
                    mp_obj_get_int_maybe( (color_components[component_ind]), &val );
                    uint8_t* location = base + ((index)*MODADD_BPL) + component_ind;

                    *(location) = (uint8_t)val;                         // set value
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

STATIC mp_obj_t addressable_layer_add_artdmx_info(mp_obj_t self_in, mp_obj_t info_in){
    addressable_layer_obj_t* self = (addressable_layer_obj_t*)MP_OBJ_TO_PTR(self_in);

    if( !mp_obj_is_type(info_in, &addressable_layer_artdmx_infoObj_type) ){
        printf("Error: the 'info_in' argument must be an ArtDMX info object type\n");
        return mp_const_none;
    }
    addressable_layer_artdmx_info_obj_t* info_template = (addressable_layer_artdmx_info_obj_t*)info_in;

    addressable_layer_artdmx_info_obj_t* info = (addressable_layer_artdmx_info_obj_t*)addressable_layer_artdmx_info_make_new(&addressable_layer_artdmx_infoObj_type,0, 0, NULL ); // Make a "copy" of the info, to try to avoid problems with multiple references to the same info
    if( !mp_obj_is_type(info, &addressable_layer_artdmx_infoObj_type) ){
        printf("Error: could not copy information from the artdmx info object\n");
        return mp_const_none;
    }
    info->cpl = info_template->cpl;
    info->leds = info_template->leds;
    info->portAddress = info_template->portAddress;
    info->start_channel = info_template->start_channel;
    info->start_index = info_template->start_index;
    info->cpl = info_template->cpl;
    info->layer = self;

    // printf("copied dmx info object with: leds = %d, portAddress = %d, start_index = %d, start_channel = %d, layer = 0x%08X\n",info->leds, info->portAddress, info->start_index, info->start_channel, (uint32_t)info->layer);
    
    

    addressable_layer_artdmx_info_node_t* node = addressable_layer_artdmx_new_info_node();
    if(node == NULL){
        printf("Error: no memory for the new node\n");
        return mp_const_none;
    }
    node->artdmx_info = info;
    info->layer = self;         // with this setup it is BAD to re-use info objects that are created by the 'addressable_layer_artdmx_info_make_new' function. B/C this only adds them 

    // if( self->artdmx_head == NULL ){
    //     self->artdmx_head = node;
    // }else{
    //     if( addressable_layer_artdmx_info_node_append( self->artdmx_head, node ) != 0){
    //         printf("Error linking the new node into the LL\n");
    //         return mp_const_none;
    //     }
    // }

    // Instead link this into a single LL of all artnet info. todo: need to consider what happens when the user deletes a layer...
    if( artdmx_info_head == NULL ){
        artdmx_info_head = node;
    }else{
        if( addressable_layer_artdmx_info_node_append( artdmx_info_head, node ) != 0){
            printf("Error linking the new node into the LL\n");
            return mp_const_none;
        } 
    }

    return mp_const_none;
}




// compose layers - for efficiency this function will operate in the context of the output protocol
typedef void (*addressable_composer_f)(addressable_fixture_obj_t* fixture, addressable_layer_obj_t* layer);

IRAM_ATTR static void addressable_composer_SET(addressable_fixture_obj_t* fixture, addressable_layer_obj_t* layer){
    uint8_t*            dst = fixture->comp_data;
    uint8_t*            src = layer->data;

    // Loop over all leds, applying necessary operation
    for(size_t led = 0; led < fixture->leds; led++){
        *(dst + (MODADD_BPL * led) + 0) = *(src + (MODADD_BPL * led) + 0);
        *(dst + (MODADD_BPL * led) + 1) = *(src + (MODADD_BPL * led) + 1);
        *(dst + (MODADD_BPL * led) + 2) = *(src + (MODADD_BPL * led) + 2);
        *(dst + (MODADD_BPL * led) + 3) = *(src + (MODADD_BPL * led) + 3);
    }
};
IRAM_ATTR static void addressable_composer_OR(addressable_fixture_obj_t* fixture, addressable_layer_obj_t* layer){
    // modadd_protocols_e  proto = fixture->ctrl->output.protocol; // using the output's protocol because we operate in the output's domain here
    // const modadd_protocol_t*  protocol = modadd_protocols[proto];
    // uint8_t             bpl = protocol->bpl;
    // uint8_t*            dst = fixture->data;
    // uint8_t*            src = layer->data;

    // for(size_t led = 0; led < fixture->leds; led++){            // Loop over all leds
    //     for(size_t channel = 0; channel < bpl; channel++){      // Loop over applicable channels (e.g. [R,G,B,A])
    //         // Map the layer [RGBA] data onto the output data [protocol dependent] and apply the specified operation
    //         *(dst + (bpl * led) + channel) |= *(src + (bpl * led) + protocol->indices[channel]);
    //     }
    // }
};
IRAM_ATTR static void addressable_composer_AND(addressable_fixture_obj_t* fixture, addressable_layer_obj_t* layer){
    // modadd_protocols_e  proto = fixture->ctrl->output.protocol; // using the output's protocol because we operate in the output's domain here
    // const modadd_protocol_t*  protocol = modadd_protocols[proto];
    // uint8_t             bpl = protocol->bpl;
    // uint8_t*            dst = fixture->data;
    // uint8_t*            src = layer->data;

    // for(size_t led = 0; led < fixture->leds; led++){            // Loop over all leds
    //     for(size_t channel = 0; channel < bpl; channel++){      // Loop over applicable channels (e.g. [R,G,B,A])
    //         // Map the layer [RGBA] data onto the output data [protocol dependent] and apply the specified operation
    //         *(dst + (bpl * led) + channel) &= *(src + (bpl * led) + protocol->indices[channel]);
    //     }
    // }
};
IRAM_ATTR static void addressable_composer_XOR(addressable_fixture_obj_t* fixture, addressable_layer_obj_t* layer){
    // modadd_protocols_e  proto = fixture->ctrl->output.protocol; // using the output's protocol because we operate in the output's domain here
    // const modadd_protocol_t*  protocol = modadd_protocols[proto];
    // uint8_t             bpl = protocol->bpl;
    // uint8_t*            dst = fixture->data;
    // uint8_t*            src = layer->data;

    // for(size_t led = 0; led < fixture->leds; led++){            // Loop over all leds
    //     for(size_t channel = 0; channel < bpl; channel++){      // Loop over applicable channels (e.g. [R,G,B,A])
    //         // Map the layer [RGBA] data onto the output data [protocol dependent] and apply the specified operation
    //         *(dst + (bpl * led) + channel) ^= *(src + (bpl * led) + protocol->indices[channel]);
    //     }
    // }
};
IRAM_ATTR static void addressable_composer_MULT(addressable_fixture_obj_t* fixture, addressable_layer_obj_t* layer){
    // modadd_protocols_e  proto = fixture->ctrl->output.protocol; // using the output's protocol because we operate in the output's domain here
    // const modadd_protocol_t*  protocol = modadd_protocols[proto];
    // uint8_t             bpl = protocol->bpl;
    // uint8_t*            dst = fixture->data;
    // uint8_t*            src = layer->data;

    // for(size_t led = 0; led < fixture->leds; led++){            // Loop over all leds
    //     for(size_t channel = 0; channel < bpl; channel++){      // Loop over applicable channels (e.g. [R,G,B,A])
    //         // Map the layer [RGBA] data onto the output data [protocol dependent] and apply the specified operation
    //         *(dst + (bpl * led) + channel) *= *(src + (bpl * led) + protocol->indices[channel]);
    //     }
    // }
};
IRAM_ATTR static void addressable_composer_DIV(addressable_fixture_obj_t* fixture, addressable_layer_obj_t* layer){
    // modadd_protocols_e  proto = fixture->ctrl->output.protocol; // using the output's protocol because we operate in the output's domain here
    // const modadd_protocol_t*  protocol = modadd_protocols[proto];
    // uint8_t             bpl = protocol->bpl;
    // uint8_t*            dst = fixture->data;
    // uint8_t*            src = layer->data;

    // for(size_t led = 0; led < fixture->leds; led++){            // Loop over all leds
    //     for(size_t channel = 0; channel < bpl; channel++){      // Loop over applicable channels (e.g. [R,G,B,A])
    //         // Map the layer [RGBA] data onto the output data [protocol dependent] and apply the specified operation
    //         *(dst + (bpl * led) + channel) /= *(src + (bpl * led) + protocol->indices[channel]);
    //     }
    // }
};
IRAM_ATTR static void addressable_composer_ADD(addressable_fixture_obj_t* fixture, addressable_layer_obj_t* layer){
    // modadd_protocols_e  proto = fixture->ctrl->output.protocol; // using the output's protocol because we operate in the output's domain here
    // const modadd_protocol_t*  protocol = modadd_protocols[proto];
    // uint8_t             bpl = protocol->bpl;
    // uint8_t*            dst = fixture->data;
    // uint8_t*            src = layer->data;

    // for(size_t led = 0; led < fixture->leds; led++){            // Loop over all leds
    //     for(size_t channel = 0; channel < bpl; channel++){      // Loop over applicable channels (e.g. [R,G,B,A])
    //         // Map the layer [RGBA] data onto the output data [protocol dependent] and apply the specified operation
    //         *(dst + (bpl * led) + channel) += *(src + (bpl * led) + protocol->indices[channel]);
    //     }
    // }
};
IRAM_ATTR static void addressable_composer_SUB(addressable_fixture_obj_t* fixture, addressable_layer_obj_t* layer){
    // modadd_protocols_e  proto = fixture->ctrl->output.protocol; // using the output's protocol because we operate in the output's domain here
    // const modadd_protocol_t*  protocol = modadd_protocols[proto];
    // uint8_t             bpl = protocol->bpl;
    // uint8_t*            dst = fixture->data;
    // uint8_t*            src = layer->data;

    // for(size_t led = 0; led < fixture->leds; led++){            // Loop over all leds
    //     for(size_t channel = 0; channel < bpl; channel++){      // Loop over applicable channels (e.g. [R,G,B,A])
    //         // Map the layer [RGBA] data onto the output data [protocol dependent] and apply the specified operation
    //         *(dst + (bpl * led) + channel) -= *(src + (bpl * led) + protocol->indices[channel]);
    //     }
    // }
};
IRAM_ATTR static void addressable_composer_COMP(addressable_fixture_obj_t* fixture, addressable_layer_obj_t* layer){
    uint8_t*            dst = fixture->comp_data;
    uint8_t*            src = layer->data;
    uint16_t            t = 0;

    // This composer composites the images using the notion of premultipled alpha information
    // Suffice it to say that a source pixel of [255,0,0,127] is invalid and wrong. Do not do. This suggests that the strength of the first color channel is well above the maximum strength.
    // In general for a given rgb channel q and an alpha (opacity) channel a you must obey q<=a, or else suffer the strange results 

    for(size_t led = 0; led < fixture->leds; led++){            // Loop over all leds
        uint8_t* src_0 = (src + (MODADD_BPL * led) + 0);
        uint8_t* src_1 = (src + (MODADD_BPL * led) + 1);
        uint8_t* src_2 = (src + (MODADD_BPL * led) + 2);
        uint8_t* src_3 = (src + (MODADD_BPL * led) + 3);        // src_3 is assumed to be the opacity

        uint8_t* dst_0 = (dst + (MODADD_BPL * led) + 0);
        uint8_t* dst_1 = (dst + (MODADD_BPL * led) + 1);
        uint8_t* dst_2 = (dst + (MODADD_BPL * led) + 2);
        uint8_t* dst_3 = (dst + (MODADD_BPL * led) + 3);

        *(dst_0) = MODADD_INT_PRELERP(*(dst_0), *(src_0), *(src_3), t );
        *(dst_1) = MODADD_INT_PRELERP(*(dst_1), *(src_1), *(src_3), t );
        *(dst_2) = MODADD_INT_PRELERP(*(dst_2), *(src_2), *(src_3), t );
        *(dst_3) = MODADD_INT_PRELERP(*(dst_3), *(src_3), *(src_3), t );

        // cap values at 255
        *(dst_0) = *(dst_0) > 255 ? 255 : *(dst_0);
        *(dst_1) = *(dst_1) > 255 ? 255 : *(dst_1);
        *(dst_2) = *(dst_2) > 255 ? 255 : *(dst_2);
        *(dst_3) = *(dst_3) > 255 ? 255 : *(dst_3);
    }
};
IRAM_ATTR static void addressable_composer_MASK(addressable_fixture_obj_t* fixture, addressable_layer_obj_t* layer){
    uint8_t*            dst = fixture->comp_data;
    uint8_t*            src = layer->data;

    for(size_t led = 0; led < fixture->leds; led++){            // Loop over all leds
            if( *(src + (MODADD_BPL * led) + 0) != 0 ){ *(dst + (MODADD_BPL * led) + 0) = 0; }
            if( *(src + (MODADD_BPL * led) + 1) != 0 ){ *(dst + (MODADD_BPL * led) + 1) = 0; }
            if( *(src + (MODADD_BPL * led) + 2) != 0 ){ *(dst + (MODADD_BPL * led) + 2) = 0; }
            if( *(src + (MODADD_BPL * led) + 3) != 0 ){ *(dst + (MODADD_BPL * led) + 3) = 0; }
    }
};

IRAM_ATTR void addressable_composer_enforce_protocol_data(addressable_fixture_obj_t* fixture){
    // todo: now this function is meant to copy the composed fixture data into the output according to all the rules of the protocol
    
    modadd_protocols_e  proto = fixture->ctrl->output.protocol; // using the output's protocol because we operate in the output's domain here
    const modadd_protocol_t*  protocol = modadd_protocols[proto];
    uint8_t             bpl = protocol->bpl;
    uint8_t*            dst = fixture->out_data;                // output data should be stored according to LED protocol rules
    uint8_t*            src = fixture->comp_data;               // composition data is stored in [RGBA] standard format

    uint8_t brightness_channel = 0xFF;                          // Check if there is a brightness channel
    if( bpl > 3 ){
        for(size_t channel = 0; channel < bpl; channel++){      // Loop over applicable channels (e.g. [R,G,B,A])
            if( protocol->indices[channel] == MODADD_A_INDEX ){
                brightness_channel = channel;
            }
        }
    }

    for(size_t led = 0; led < fixture->leds; led++){            // Loop over all leds
        for(size_t channel = 0; channel < bpl; channel++){      // Loop over applicable channels (e.g. [R,G,B,A])
            // Map the layer [RGBA] data onto the output data [protocol dependent] and apply the specified operation
            *(dst + (bpl * led) + channel) = *(src + (bpl * led) + protocol->indices[channel]);
            *(dst + (bpl * led) + channel) |= protocol->or_mask[channel];           // set mask
        }
    }    

    if( brightness_channel < bpl ){
        for(size_t led = 0; led < fixture->leds; led++){            // Loop over all leds
            *(dst + (bpl * led) + brightness_channel) = (fixture->brightness >> protocol->brightness_rightshifts);  // apply the brightness
            *(dst + (bpl * led) + brightness_channel) |= protocol->or_mask[brightness_channel];                     // re-apply the mask
        } 
    }
    
    // protocol enforcement todo: enforce trailing sequences
}

IRAM_ATTR void addressable_layer_compose(void* arg){
    modadd_ctrl_t* ctrl = (modadd_ctrl_t*)arg;

    addressable_fixture_obj_t* fixture = NULL;
    modadd_fixture_iter_t fiter = NULL;

    addressable_layer_obj_t* layer = NULL;
    modadd_layer_iter_t liter = NULL;
    modadd_operations_e op = MODADD_OP_NUM;

    addressable_composer_f composer = NULL;
    
    if(ctrl->fixture_ctrl.head == NULL){ return; } // bail early if there are no fixtures
    if(ctrl->fixture_ctrl.data == NULL){ return; } // also bail if there is no output data to work with
    if(ctrl->fixture_ctrl.data_len == 0){ return; }

    // Otherwise... iterate over fixtures linked list 
    for( fiter = modadd_fixture_iter_first(MODADD_ITER_FROM_FIXTURE_PTR(ctrl->fixture_ctrl.head)); !modadd_fixture_iter_done(fiter); fiter = modadd_fixture_iter_next(fiter) ){
        fixture = (addressable_fixture_obj_t*)MODADD_FIXTURE_PTR_FROM_ITER(fiter)->fixture;

        if( fixture == NULL ){ continue; } // a few scenarios (which should rarely happen) that prompt us to skip this fixture
        if( fixture->layers == NULL ){ continue; }
        if( fixture->out_data == NULL ){ continue; }
        if( fixture->comp_data == NULL ){ continue; }
        if( fixture->leds == 0 ){ continue; }

        // Zero out the composition buffer (this is a solution to most "second composition" problems)
        memset(fixture->comp_data, 0x00, (MODADD_BPL*fixture->leds)*sizeof(uint8_t) ); //zero-out the composition buffer

        // For each layer in the fixture use the specified operation to combine with the output data
        for( liter = modadd_layer_iter_first(MODADD_ITER_FROM_LAYER_PTR( fixture->layers )); !modadd_layer_iter_done(liter); liter = modadd_layer_iter_next(liter) ){
            layer = (addressable_layer_obj_t*)MODADD_LAYER_PTR_FROM_ITER(liter)->layer;
            op = layer->op;
            if( layer->data == NULL ){ continue; } // can't touch this layer if it has no data

            if(op != MODADD_OP_NUM){ // MODADD_OP_NUM is entirely invalid, we will skip data in this layer 

                switch( op ){
                    case MODADD_OP_SET  : composer = addressable_composer_SET; break;
                    case MODADD_OP_OR   : composer = addressable_composer_OR; break;
                    case MODADD_OP_AND  : composer = addressable_composer_AND; break;
                    case MODADD_OP_XOR  : composer = addressable_composer_XOR; break;
                    case MODADD_OP_MULT : composer = addressable_composer_MULT; break;
                    case MODADD_OP_DIV  : composer = addressable_composer_DIV; break;
                    case MODADD_OP_ADD  : composer = addressable_composer_ADD; break;
                    case MODADD_OP_SUB  : composer = addressable_composer_SUB; break;
                    case MODADD_OP_COMP : composer = addressable_composer_COMP; break;
                    case MODADD_OP_MASK : composer = addressable_composer_MASK; break;

                    case MODADD_OP_SKIP :
                    case MODADD_OP_NUM :
                    default :
                        composer = NULL;
                        break;
                }

                if( composer != NULL ){
                    composer( fixture, layer ); // call the composer
                }
            }
        }
        // After mixing all the channels we will perform protocol enforcement
        addressable_composer_enforce_protocol_data(fixture);
    }
    // Once all fixtures have been mixed and had their data enforced we could perform leading/trailing sequence enforcement
    // And after that the data is completely ready to send out!
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