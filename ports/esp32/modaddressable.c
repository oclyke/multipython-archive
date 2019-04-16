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

*/
#include "modaddressable.h"

////////////////////////////////////////////////////////////////////////////
/* MicroPython Addressable Control Class                                  */
////////////////////////////////////////////////////////////////////////////

STATIC mp_obj_t addressable_controller_add_fixture(mp_obj_t self_in, mp_obj_t fixture_obj);
MP_DEFINE_CONST_FUN_OBJ_2(addressable_controller_add_fixture_obj, addressable_controller_add_fixture);

STATIC mp_obj_t addressable_controller_recompute_chain(mp_obj_t self_in);
MP_DEFINE_CONST_FUN_OBJ_1(addressable_controller_recompute_chain_obj, addressable_controller_recompute_chain);

STATIC void addressable_controller_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind );
mp_obj_t addressable_controller_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args );

STATIC const mp_rom_map_elem_t addressable_controller_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_add_fixture), MP_ROM_PTR(&addressable_controller_add_fixture_obj) },
    { MP_ROM_QSTR(MP_QSTR_recompute_chain), MP_ROM_PTR(&addressable_controller_recompute_chain_obj) },
    
 };
STATIC MP_DEFINE_CONST_DICT(addressable_controller_locals_dict, addressable_controller_locals_dict_table);


// define the controller class-object
const mp_obj_type_t addressable_controllerObj_type = {
    { &mp_type_type },                              // "inherit" the type "type"
    .name = MP_QSTR_controllerObj,                     // give it a name
    .print = addressable_controller_print,             // give it a print-function
    .make_new = addressable_controller_make_new,       // give it a constructor
    .locals_dict = (mp_obj_dict_t*)&addressable_controller_locals_dict, // and the global members
};

// this is the actual C-structure for our new object
typedef struct _addressable_controller_obj_t {
    mp_obj_base_t     base;         // base represents some basic information, like type
    const uint8_t     id;           // id, corresponds to index in 'modadd_controllers' and the value of the corresponding modadd_controllers_e
    modadd_ctrl_t*    info;         // the controller info - points at the actual controller info structure
} addressable_controller_obj_t;





// define micropython controller objects based on the addressable_controllers structures
addressable_controller_obj_t mach1_controller_stat_obj = {
    .base = { .type = &addressable_controllerObj_type, },
    .id = (uint8_t)MACH1_CONTROLLER_STAT,
    // .info = modadd_controllers[MACH1_CONTROLLER_STAT],
};

addressable_controller_obj_t mach1_controller_aled_obj = {
    .base = { .type = &addressable_controllerObj_type, },
    .id = (uint8_t)MACH1_CONTROLLER_ALED,
    // .info = modadd_controllers[MACH1_CONTROLLER_STAT],
};

addressable_controller_obj_t* addressable_controllers[] = { // these must follow the same order as the 'modadd_controllers_e' enum and the 'modadd_controllers' array
    &mach1_controller_stat_obj,
    &mach1_controller_aled_obj,
};





mp_obj_t addressable_controller_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args ) {
    enum { ARG_id, ARG_protocol };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_id,       MP_ARG_REQUIRED | MP_ARG_INT,    {.u_int = MACH1_CONTROLLER_STAT} },
        { MP_QSTR_protocol,                   MP_ARG_INT,    {.u_int = MODADD_PROTOCOL_UNKNOWN } },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    
    // this checks the number of arguments (min 1, max 1);
    // on error -> raise python exception
    mp_arg_check_num(n_args, n_kw, 1, 2, true);

    // According to the ID value get the corresponding output controller
    if( args[ARG_id].u_int >= MACH1_CONTROLLERS_NUM ){
        mp_raise_ValueError("ID exceeds number of available controllers\n");
        return mp_const_none;
    }

    // Make sure existing controller settings are copied over before changing anything. Pay attention, the index numbers are very important!
    addressable_controller_obj_t* self = addressable_controllers[args[ARG_id].u_int];
    self->info = modadd_controllers[self->id];

    // If possible take the user's desired protocol and set it in the controller
    if( args[ARG_protocol].u_int < MODADD_PROTOCOLS_NUM ){
        if( args[ARG_id].u_int == MACH1_CONTROLLER_STAT ){
            if( args[ARG_protocol].u_int != MODADD_PROTOCOL_UNKNOWN ){
                printf("Warning: protocol ignored for STAT LED output controller\n");
            }
        }else{
            self->info->output.protocol = args[ARG_protocol].u_int;
        }
    }
    return MP_OBJ_FROM_PTR(self);
}

STATIC void addressable_controller_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind ) {
    // get a ptr to the C-struct of the object
    addressable_controller_obj_t *self = MP_OBJ_TO_PTR(self_in);
    // print the number
    printf ("Addressable controller class object with id = %d, and protocol = %d\n", self->id, self->info->output.protocol);
}

STATIC mp_obj_t addressable_controller_add_fixture(mp_obj_t self_in, mp_obj_t fixture_obj) {
    addressable_controller_obj_t *self = MP_OBJ_TO_PTR(self_in);
    addressable_fixture_obj_t* fixture = MP_OBJ_TO_PTR(fixture_obj);

    if( self->info->output.protocol != fixture->info.protocol ){
        mp_raise_ValueError("Protocol mismatch, cannot add fixture\n");
        return mp_const_none;
    }

    if ( self->info->fixture_ctrl.append == NULL ){
        mp_raise_ValueError("Adding fixtures not permitted for this controller\n");
        return mp_const_none;       
    }

    // Append the fixture through the specified append function
    modadd_status_e status = self->info->fixture_ctrl.append( &(fixture->info), self->info );
    if( status != MODADD_STAT_OK ){
        printf("Appending failed with code %d\n", status);
        return mp_const_none;
    }
    return mp_const_none;
}


STATIC mp_obj_t addressable_controller_recompute_chain(mp_obj_t self_in){
    addressable_controller_obj_t *self = MP_OBJ_TO_PTR(self_in);
    modadd_ctrl_recompute_fixtures( self->info );
    return mp_const_none;
}













































// // controller functions
// modadd_status_e modadd_output_verify_memory( void ){ // todo: fill out stub code (probably needs arguments!!!)

//     return MODADD_STAT_ERR;

//     return MODADD_STAT_OK;
// }


modadd_status_e modadd_control_start( modadd_ctrl_t* ctrl ){
    if( ctrl == NULL ){ return MODADD_STAT_ERR; }
    modadd_status_e ret = MODADD_STAT_OK;
    ret = modadd_output_initialize( ctrl );
    if( ret != MODADD_STAT_OK ){
        // todo: de-initialize output
        return ret;
    }
    ret = modadd_timer_start( ctrl );
    if( ret != MODADD_STAT_OK ){
        // todo: de-initialize output
        // todo: deinitialize timer
        return ret;
    }
    return MODADD_STAT_OK;
}


// interface 
STATIC mp_obj_t modadd_start( size_t n_args, const mp_obj_t *args ){
    // start all outputs
    // optionally, give a list of outputs to start

    mp_obj_t started_outputs = mp_obj_new_list( 0, NULL );

    switch( n_args ){
        case 1:
            if(mp_obj_is_int(args[0])){
                uint8_t output_index = mp_obj_int_get_truncated(args[0]);
                if( output_index < MODADD_NUM_CONTROLLERS ){
                    if( modadd_control_start( (modadd_controllers[output_index]) ) != MODADD_STAT_OK ){}
                    else{ mp_obj_list_append(started_outputs, mp_obj_new_int(output_index) ); }
                }
            }else if( mp_obj_is_type(args[0], &mp_type_list) ){
                size_t size;
                mp_obj_t* items;
                mp_obj_list_get(args[0], &size, &items );
                for( size_t indi = 0; indi < size; indi++ ){
                    if( !mp_obj_is_int(items[indi]) ){
                        mp_raise_TypeError("list element of non-integer type");
                        return mp_const_none;
                    }
                }
                for( size_t indi = 0; indi < size; indi++ ){
                    uint8_t output_index = mp_obj_int_get_truncated(items[indi]);
                    if( output_index < MODADD_NUM_CONTROLLERS ){
                        if( modadd_control_start( (modadd_controllers[output_index]) ) != MODADD_STAT_OK ){}
                        else{ mp_obj_list_append(started_outputs, mp_obj_new_int(output_index) ); }
                    }
                }
            }else{
                mp_raise_TypeError("expects integer or list of integers");
                return mp_const_none;
            }
            break;

        case 0:
        default:
            // for(iter = mp_context_iter_next(mp_context_iter_first(mp_context_head)); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
            //     multipython_end_task( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id );
            //     mp_obj_list_append(stopped_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
            // }
            break;
    }

    return started_outputs;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(modadd_start_obj, 0, 1, modadd_start);



// Module Definitions
STATIC const mp_rom_map_elem_t mp_module_addressable_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_addressable) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&modadd_start_obj) },

    { MP_ROM_QSTR(MP_QSTR_controller), MP_ROM_PTR(&addressable_controllerObj_type) },
    { MP_ROM_QSTR(MP_QSTR_fixture), MP_ROM_PTR(&addressable_fixtureObj_type) },

    { MP_ROM_QSTR(MP_QSTR_STAT_CONTROLLER), MP_ROM_INT(MACH1_CONTROLLER_STAT) },
    { MP_ROM_QSTR(MP_QSTR_ALED_CONTROLLER), MP_ROM_INT(MACH1_CONTROLLER_ALED) },


    // { MP_ROM_QSTR(MP_QSTR_get_outputs), MP_ROM_PTR(&modadd_get_outputs_obj) },

    // { MP_ROM_QSTR(MP_QSTR_green), MP_ROM_PTR(&addressable_green_obj) },
    // { MP_ROM_QSTR(MP_QSTR_blue), MP_ROM_PTR(&addressable_blue_obj) },
    // { MP_ROM_QSTR(MP_QSTR_stat), MP_ROM_PTR(&modadd_stat_obj) },
    // { MP_ROM_QSTR(MP_QSTR_strip), MP_ROM_PTR(&modadd_strip_obj) },
    // { MP_ROM_QSTR(MP_QSTR_link_artnet), MP_ROM_PTR(&modadd_link_artnet_obj) },
    // { MP_ROM_QSTR(MP_QSTR_add_led_strip), MP_ROM_PTR(&modadd_add_led_strip_obj) },
};
STATIC MP_DEFINE_CONST_DICT(mp_module_addressable_globals, mp_module_addressable_globals_table);

const mp_obj_module_t mp_module_addressable = { 
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_addressable_globals,
};

























typedef struct _modadd_fixture_recomputation_struct_t{
    modadd_ctrl_t*              ctrl;
    uint8_t*                    data;
    uint32_t                    total_leds;
    const modadd_protocol_t*    protocol;
    bool                        protocol_ok;
}modadd_fixture_recomputation_struct_t;

void modadd_fixture_count_leds(modadd_fixture_iter_t iter, void* args){
    modadd_fixture_recomputation_struct_t* recomp = (modadd_fixture_recomputation_struct_t*)args;
    if( modadd_protocols[MODADD_FIXTURE_PTR_FROM_ITER(iter)->protocol] != recomp->protocol ){
        printf("Fixture protocol differs from output protocol! Fixture at 0x%X\n", (uint32_t)MODADD_FIXTURE_PTR_FROM_ITER(iter) );
        recomp->protocol_ok = false;
    }
    recomp->total_leds += (MODADD_FIXTURE_PTR_FROM_ITER(iter)->leds);
}

void modadd_fixture_recomputation(modadd_fixture_iter_t iter, void* args){
    modadd_fixture_recomputation_struct_t* recomp = (modadd_fixture_recomputation_struct_t*)args;

    MODADD_FIXTURE_PTR_FROM_ITER(iter)->data = recomp->data;
    MODADD_FIXTURE_PTR_FROM_ITER(iter)->ctrl = recomp->ctrl;
    recomp->data += ((MODADD_FIXTURE_PTR_FROM_ITER(iter)->leds) * recomp->protocol->bpl);
    // recomp
}

void modadd_ctrl_recompute_fixtures( modadd_ctrl_t* ctrl ){
    // recompute dynamic members of fixtures in the output string for ctrl
    const modadd_protocol_t* protocol = modadd_protocols[ctrl->output.protocol];
    modadd_fixture_recomputation_struct_t recomp;
    recomp.protocol = protocol;
    recomp.total_leds = 0;
    recomp.protocol_ok = true;

    // count the leds total
    modadd_fixture_foreach(ctrl->fixture_ctrl.head, modadd_fixture_count_leds, (void*)&recomp);
    if( recomp.protocol_ok != true ){
        printf("Protocol failure. Cannot continue with chain recomputation\n");
        return;
    }
    printf("Total LEDs in the chain: %d\n", recomp.total_leds);

    size_t length_required = (recomp.total_leds * protocol->bpl) + protocol->num_leading + protocol->num_trailing;
    if( ctrl->fixture_ctrl.data == NULL ){
        // Need to allocate memory regardless
        printf("First memory allocation for this controller.\n");
        uint8_t* data = (uint8_t*)MODADD_MALLOC_DMA( length_required * sizeof(uint8_t) );
        if( data == NULL ){
            printf("allocation failed. Please reduce the number of fixtures on this string and try again\n");
            return;
        }
        ctrl->fixture_ctrl.data = data;
    }else{
        size_t current_allocation = ctrl->fixture_ctrl.data_len;
        if( current_allocation < length_required ){
            printf("Need to increase memory allocation for the controller.\n");
            MODADD_FREE( ctrl->fixture_ctrl.data );
            uint8_t* data = (uint8_t*)MODADD_MALLOC_DMA( length_required * sizeof(uint8_t) );
            if( data == NULL ){
                printf("allocation failed. Please reduce the number of fixtures on this string and try again\n");
                return;
            }
            ctrl->fixture_ctrl.data = data;
        }
    }

    printf("Output chain memory beginning at 0x%X\n", (uint32_t)ctrl->fixture_ctrl.data );
    // Now update the fixture control structure as needed
    ctrl->fixture_ctrl.data_len = length_required;
    memset((void*)ctrl->fixture_ctrl.data, 0x00, length_required); // zero out the new memory

    // Prepare to do the second phase of recomputation - setting fixture data offsets and linking back to the ctrl structure    
    recomp.ctrl = ctrl;
    recomp.data = ctrl->fixture_ctrl.data + protocol->num_leading;
    modadd_fixture_foreach(ctrl->fixture_ctrl.head, modadd_fixture_recomputation, (void*)&recomp);    

    // now apply the mask for this protocol to all applicable memory locations (note: this will probably clear all LED data)
    uint8_t* base = ctrl->fixture_ctrl.data + protocol->num_leading;
    for(uint32_t led_ind = 0; led_ind < recomp.total_leds; led_ind++){
        for(uint8_t component_ind = 0; component_ind < protocol->bpl; component_ind++ ){
            uint8_t* location = base + ((led_ind)*(protocol->bpl)) + component_ind;
            *(location) |= protocol->or_mask[component_ind];    // set mask
        }
    }

    printf("Recomputed chain :)\n");
}














































// STATIC mp_obj_t modadd_get_outputs( size_t n_args, const mp_obj_t *args ){
//     // get output info for all outputs
//     // optionally, provide a intger or list of integers to specify outputs for which to get information

//     mp_obj_t started_outputs = mp_obj_new_list( 0, NULL );

//     switch( n_args ){
//         case 1:
//             if(mp_obj_is_int(args[0])){
//                 uint8_t output_index = mp_obj_int_get_truncated(args[0]);
//                 if( output_index < MODADD_NUM_OUTPUTS ){
//                     if( modadd_output_start( &(modadd_controllers[output_index]) ) != MODADD_STAT_OK ){}
//                     else{ mp_obj_list_append(started_outputs, mp_obj_new_int(output_index) ); }
//                 }
//             }else if( mp_obj_is_type(args[0], &mp_type_list) ){
//                 size_t size;
//                 mp_obj_t* items;
//                 mp_obj_list_get(args[0], &size, &items );
//                 for( size_t indi = 0; indi < size; indi++ ){
//                     if( !mp_obj_is_int(items[indi]) ){
//                         mp_raise_TypeError("list element of non-integer type");
//                         return mp_const_none;
//                     }
//                 }
//                 for( size_t indi = 0; indi < size; indi++ ){
//                     uint8_t output_index = mp_obj_int_get_truncated(items[indi]);
//                     if( output_index < MODADD_NUM_OUTPUTS ){
//                         if( modadd_output_start( &(modadd_controllers[output_index]) ) != MODADD_STAT_OK ){}
//                         else{ mp_obj_list_append(started_outputs, mp_obj_new_int(output_index) ); }
//                     }
//                 }
//             }else{
//                 mp_raise_TypeError("expects integer or list of integers");
//                 return mp_const_none;
//             }
//             break;

//         case 0:
//         default:
//             // for(iter = mp_context_iter_next(mp_context_iter_first(mp_context_head)); !mp_context_iter_done(iter); iter = mp_context_iter_next(iter)){
//             //     multipython_end_task( (uint32_t)MP_CONTEXT_PTR_FROM_ITER(iter)->id );
//             //     mp_obj_list_append(stopped_contexts, mp_obj_new_int(MP_CONTEXT_PTR_FROM_ITER(iter)->id) );
//             // }
//             break;
//     }

//     return started_outputs;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(modadd_start_obj, 0, 1, modadd_start);

// modadd_get_outputs_obj


// mp_obj_t modadd_stat( mp_obj_t red, mp_obj_t blue, mp_obj_t green ){
//     uint8_t brightness = 0b00010000;
//     //                     xxx
//     machone_stat_data[0] = 0;
//     machone_stat_data[1] = 0;
//     machone_stat_data[2] = 0;
//     machone_stat_data[3] = 0;
//     machone_stat_data[1*4 + 0] = 0xE0 | brightness;
//     machone_stat_data[1*4 + 1] = mp_obj_int_get_truncated(blue);
//     machone_stat_data[1*4 + 2] = mp_obj_int_get_truncated(green);
//     machone_stat_data[1*4 + 3] = mp_obj_int_get_truncated(red);
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_3(modadd_stat_obj, modadd_stat);

// mp_obj_t modadd_strip( mp_obj_t red, mp_obj_t blue, mp_obj_t green ){

//     modadd_ctrl_t* aled = &modadd_controllers[0];

//     if( aled->fixture_ctrl.data_len < 4){
//         printf("there's too little data in the fixture control block! %d bytes\n", aled->fixture_ctrl.data_len);
//         return mp_const_none;
//     }

//     uint8_t* data = (uint8_t*)aled->fixture_ctrl.data;

//     if(aled->fixture_ctrl.data_len > 3){
//         data[0] = 0;
//         data[1] = 0;
//         data[2] = 0;
//         data[3] = 0;
//     }

//     uint32_t num_leds = ((aled->fixture_ctrl.data_len / 4) - 2);
//     uint8_t brightness = 0b00010000;
//     //                     xxx
//     for( uint32_t indi = 0; indi < num_leds; indi++ ){
//         data[(indi+1)*4 + 0] = 0xE0 | brightness;
//         data[(indi+1)*4 + 1] = mp_obj_int_get_truncated(blue);
//         data[(indi+1)*4 + 2] = mp_obj_int_get_truncated(green);
//         data[(indi+1)*4 + 3] = mp_obj_int_get_truncated(red);
//     }

//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_3(modadd_strip_obj, modadd_strip);

// mp_obj_t addressable_green( void ){
//     uint8_t brightness = 0b00010000;
//     //                     xxx
//     led_data[(4*0)+ 0] = 0;
//     led_data[(4*0)+ 1] = 0;
//     led_data[(4*0)+ 2] = 0;
//     led_data[(4*0)+ 3] = 0;
//     for(uint32_t indi = 0; indi < NUM_LEDS; indi++){
//         led_data[(4*(indi+1))+ 0] = 0xE0 | brightness;
//         led_data[(4*(indi+1))+ 1] = 0;
//         led_data[(4*(indi+1))+ 2] = 0xFF;
//         led_data[(4*(indi+1))+ 3] = 0;
//     }
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_0(addressable_green_obj, addressable_green);

// mp_obj_t addressable_blue( void ){
//     uint8_t brightness = 0b00010000;
//     //                     xxx
//     led_data[(4*0)+ 0] = 0;
//     led_data[(4*0)+ 1] = 0;
//     led_data[(4*0)+ 2] = 0;
//     led_data[(4*0)+ 3] = 0;
//     for(uint32_t indi = 0; indi < NUM_LEDS; indi++){
//         led_data[(4*(indi+1))+ 0] = 0xE0 | brightness;
//         led_data[(4*(indi+1))+ 1] = 0xFF;
//         led_data[(4*(indi+1))+ 2] = 0;
//         led_data[(4*(indi+1))+ 3] = 0;
//     }
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_0(addressable_blue_obj, addressable_blue);


// void stat_artnet_callback( void* args ){
//     uint8_t brightness = 0b00010000;
//     //                     xxx
//     machone_stat_data[0] = 0;
//     machone_stat_data[1] = 0;
//     machone_stat_data[2] = 0;
//     machone_stat_data[3] = 0;
//     machone_stat_data[1*4 + 0] = 0xE0 | brightness;
//     machone_stat_data[1*4 + 1] = artnet_packet.Data[0];
//     machone_stat_data[1*4 + 2] = artnet_packet.Data[1];
//     machone_stat_data[1*4 + 3] = artnet_packet.Data[2];

//     machone_stat_timer_callback( NULL );
// }

// mp_obj_t modadd_link_artnet( void ){
//     artnet_add_callback_c_args( stat_artnet_callback, NULL );
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_0(modadd_link_artnet_obj, modadd_link_artnet);

// mp_obj_t modadd_add_led_strip( void ){

//     modadd_output_t* aled = &modadd_controllers[0];
//     modadd_fixture_t* fixture = modadd_fixture_append_new( &(aled->fixture_ctrl) );
//     if( fixture == NULL ){
//         printf("hmm, could not append a new fixture");
//         return mp_const_none;
//     }

//     fixture->leds = 144;
//     // fixture->leds = 10;
//     fixture->name = "my fixture";
//     fixture->protocol = MODADD_PROTOCOL_APA102_HW;

//     printf("new fixture data:\n");
//     static int new_fixture_count = 0;

//     modadd_fixture_iter_t iter = NULL;
//     for( iter = modadd_fixture_iter_first(MODADD_ITER_FROM_FIXTURE_PTR(aled->fixture_ctrl.head)); !modadd_fixture_iter_done(iter); iter = modadd_fixture_iter_next(iter) ){
//         printf("pos: %d, name: %s, leds: %d, protocol: %d\n", new_fixture_count++, fixture->name, fixture->leds, fixture->protocol );
//     }

//     printf("\n");

//     // todo: should make sure that memory is correctly aligned for DMA, and won't drop any bytes

//     size_t size = (fixture->leds + 2)*4;

//     uint8_t* newdata = NULL;
//     newdata = (uint8_t*)heap_caps_malloc( size, MALLOC_CAP_DMA );
//     if( newdata == NULL ){
//         printf("could not allocate memory for %d leds", fixture->leds);
//         return mp_const_none;
//     }

//     aled->fixture_ctrl.data_len = size;
//     aled->fixture_ctrl.data = newdata;


//     aled->output.protocol = MODADD_PROTOCOL_APA102_HW; // there should be a better way to do this, but it works for now


//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_0(modadd_add_led_strip_obj, modadd_add_led_strip);
