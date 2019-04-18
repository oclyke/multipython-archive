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

#include "modaddressable_timer.h"


////////////////////////////////////////////////////////////////////////////
/* MicroPython Timer Class                                              */
////////////////////////////////////////////////////////////////////////////

mp_obj_t addressable_timer_start( mp_obj_t self_in );
MP_DEFINE_CONST_FUN_OBJ_1(addressable_timer_start_obj, addressable_timer_start);

mp_obj_t addressable_timer_stop( mp_obj_t self_in );
MP_DEFINE_CONST_FUN_OBJ_1(addressable_timer_stop_obj, addressable_timer_stop);

STATIC mp_obj_t addressable_timer_period(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(addressable_timer_period_obj, 0, addressable_timer_period);

STATIC void addressable_timer_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind );
mp_obj_t addressable_timer_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args );


STATIC const mp_rom_map_elem_t addressable_timer_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&addressable_timer_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&addressable_timer_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_period), MP_ROM_PTR(&addressable_timer_period_obj) },
 };
STATIC MP_DEFINE_CONST_DICT(addressable_timer_locals_dict, addressable_timer_locals_dict_table);


// define the fixture class-object
const mp_obj_type_t addressable_timerObj_type = {
    { &mp_type_type },                              // "inherit" the type "type"
    .name = MP_QSTR_timerObj,                     // give it a name
    .print = addressable_timer_print,             // give it a print-function
    .make_new = addressable_timer_make_new,       // give it a constructor
    .locals_dict = (mp_obj_dict_t*)&addressable_timer_locals_dict, // and the global members
};


mp_obj_t addressable_timer_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args ) {
    enum { /* ARG_period, ARG_c_ptr, */ ARG_controller_id };
    static const mp_arg_t allowed_args[] = {
        // { MP_QSTR_period,   MP_ARG_REQUIRED | MP_ARG_INT,    {.u_int = 33333 } },
        // { MP_QSTR_c_ptr,    MP_ARG_KW_ONLY  | MP_ARG_INT,    {.u_int = 0 } },
        { MP_QSTR_controller_id,   MP_ARG_REQUIRED | MP_ARG_INT,    {.u_int = -1 } },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if( args[ARG_controller_id].u_int >= MODADD_NUM_CONTROLLERS ){
        mp_raise_ValueError("Invalid controller ID, cannot get timer.\n");
    }

    // get relevant info / structures
    modadd_ctrl_t* ctrl = modadd_controllers[args[ARG_controller_id].u_int];

    // create a new object of our C-struct type
    addressable_timer_obj_t *self = m_new_obj(addressable_timer_obj_t);
    self->base.type = &addressable_timerObj_type;   // give it a type
    self->info = &(ctrl->timer);
    self->info->create_args.arg = ctrl;             // guarantee that the timer has the correct controller linked in    
    
    return MP_OBJ_FROM_PTR(self);
}

STATIC void addressable_timer_print( const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind ) {
    addressable_timer_obj_t *self = MP_OBJ_TO_PTR(self_in); // get a ptr to the C-struct of the object
    printf ("timer class object with period = %d. (may be truncated)\n", (uint32_t)self->info->period );
}

mp_obj_t addressable_timer_start( mp_obj_t self_in ){
    addressable_timer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    esp_err_t ret;
    if( self->info->timer_handle == NULL ){
        // printf("before creating a timer. args ptr = 0x%X, handle pointer = 0x%x\n", (uint32_t)&(self->info->create_args) , (uint32_t)&(self->info->timer_handle) );
        ESP_ERROR_CHECK( esp_timer_create( &(self->info->create_args), &(self->info->timer_handle)));
    }
    // printf("before starting a timer. handle pointer = 0x%x, period = %d\n", (uint32_t)&(self->info->timer_handle), (uint32_t)self->info->period );
    ret = esp_timer_start_periodic( self->info->timer_handle, self->info->period );
    if( ret != ESP_OK ){
        mp_raise_ValueError("error starting the timer\n");
    }
    return mp_const_none;
}

mp_obj_t addressable_timer_stop( mp_obj_t self_in ){
    addressable_timer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    esp_err_t ret;
    if( self->info->timer_handle == NULL ){
        mp_raise_ValueError("no timer to stop\n");
    }
    ret = esp_timer_stop( self->info->timer_handle );
    if( ret != ESP_OK ){
        mp_raise_ValueError("error stopping the timer\n");
    }
    return mp_const_none;
}

STATIC mp_obj_t addressable_timer_period(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self_in, ARG_period };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_self_in,  MP_ARG_REQUIRED | MP_ARG_OBJ,   {.u_obj = mp_const_none} },
        { MP_QSTR_period,   MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);


    // mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    // mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if( mp_obj_is_type( args[ARG_self_in].u_obj, &mp_type_NoneType ) ){
        mp_raise_TypeError("the first argument 'self' must be a timer class object\n");
    }

    addressable_timer_obj_t *self = MP_OBJ_TO_PTR(args[ARG_self_in].u_obj);

    if( !mp_obj_is_type( args[ARG_period].u_obj, &mp_type_NoneType ) ){
        if( !mp_obj_is_int( args[ARG_period].u_obj ) ){
            mp_raise_TypeError("'period' must be an integer\n");
        }else{
            self->info->period = mp_obj_int_get_truncated( args[ARG_period].u_obj );
        }
    }

    // addressable_timer_start( args[ARG_self_in].u_obj );

    return mp_const_none;
}

// STATIC mp_obj_t addressable_timer_set(mp_obj_t self_in, mp_obj_t start_index_obj, mp_obj_t colors) {
//     addressable_timer_obj_t *self = MP_OBJ_TO_PTR(self_in);

//     if( self->info.protocol >= MODADD_PROTOCOLS_NUM ){
//         mp_raise_ValueError("Fixture protocol unknown, cannot set data\n");
//         return mp_const_none;
//     }

//     mp_int_t start_index;
//     mp_obj_get_int_maybe( start_index_obj, &start_index );

//     // printf("index = %d\n", index);

//     if( start_index >= self->info.leds ){ 
//         mp_raise_ValueError("Index exceeds fixture LEDs\n"); 
//         return mp_const_none;
//     }

//     if( mp_obj_is_type( colors, &mp_type_list) ){
//         size_t colors_len;
//         mp_obj_t* colors_items;
//         mp_obj_list_get(colors, &colors_len, &colors_items);

//         // printf("the number of colors supplied is: %d\n", colors_len);
//         // printf("the address of the first color is: 0x%X\n", (uint32_t)colors_items);

//         const modadd_protocol_t* protocol = modadd_protocols[self->info.protocol];
//         uint8_t     bpl = protocol->bpl;
//         uint8_t*    base = self->info.data;

//         // printf("base address for this fixture = 0x%X\n", (uint32_t)base );

//         for(size_t color_ind = 0; color_ind < colors_len; color_ind++){
//             if( mp_obj_is_type( colors_items[color_ind], &mp_type_list) ){
//                 size_t color_len;
//                 mp_obj_t* color_components;
//                 mp_obj_list_get(colors_items[color_ind], &color_len, &color_components);

                

//                 uint32_t index = color_ind + start_index;
//                 if( index >= (self->info.leds) ){ break; } // don't write into other fixture's memory!

//                 if( ( base == NULL ) ){ mp_raise_ValueError("There is no memory for this fixture - try adding the fixture to an output string\n"); }
//                 if( color_len < bpl ){
//                     mp_raise_ValueError("Too few color elements for this protocol\n");
//                     return mp_const_none;
//                 }

//                 for( uint8_t component_ind = 0; component_ind < bpl; component_ind++ ){
//                     mp_int_t val;
//                     mp_obj_get_int_maybe( (color_components[protocol->indices[component_ind]]), &val );

//                     uint8_t* location = base + ((index)*bpl) + component_ind;

//                     *(location) = (uint8_t)val;                         // set value
//                     *(location) |= protocol->or_mask[component_ind];    // set mask

//                     // printf("component_ind = %d, final value = %d, location = 0x%X\n", component_ind, (uint8_t)(*location), (uint32_t)location );
//                 }
//                 // printf("\n");

//             }else{
//                 mp_raise_TypeError("Elements of 'colors' should be lists of integers\n");
//             }
//         }
//     }else{
//         mp_raise_TypeError("color should be a list of lists of integers\n");
//         return mp_const_none;
//     }

//     return mp_const_none;
// }





































// modadd_status_e modadd_timer_start( modadd_ctrl_t* ctrl ){
//     if( ctrl == NULL ){ return MODADD_STAT_ERR; }
//     modadd_output_timer_t* timer = &(ctrl->timer);

//     // esp_timer_create_args_t timer_args;
//     // timer_args.callback = timer->callback;
//     // timer_args.arg = ctrl; // provide the control structure in the callback

//     ESP_ERROR_CHECK(esp_timer_create(&(self->create_args), &(timer->timer_handle)));
//     ESP_ERROR_CHECK(esp_timer_start_periodic(timer->timer_handle, timer->period));

//     return MODADD_STAT_OK;
// }