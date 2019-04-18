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

#ifndef _MODADDRESSABLE_TIMER_H_
#define _MODADDRESSABLE_TIMER_H_

#include "modaddressable.h"

extern const mp_obj_type_t addressable_timerObj_type;

mp_obj_t addressable_timer_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args );
mp_obj_t addressable_timer_start( mp_obj_t self_in );
mp_obj_t addressable_timer_stop( mp_obj_t self_in );

// this is the actual C-structure for our new object
typedef struct _addressable_timer_obj_t {
    mp_obj_base_t           base;           // base represents some basic information, like type
    modadd_output_timer_t*  info;           // pointer to the underlying esp timer
} addressable_timer_obj_t;

// modadd_status_e modadd_timer_start( modadd_ctrl_t* ctrl );


#endif // _MODADDRESSABLE_TIMER_H_