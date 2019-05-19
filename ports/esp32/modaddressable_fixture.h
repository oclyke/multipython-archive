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

#ifndef _MODADDRESSABLE_FIXTURE_H_
#define _MODADDRESSABLE_FIXTURE_H_

#include "modaddressable.h"

extern const mp_obj_type_t addressable_fixtureObj_type;

// fixture function forward declarations
void modadd_ctrl_recompute_fixtures( modadd_ctrl_t* ctrl );
modadd_status_e modadd_fixture_append( modadd_fixture_t* node, modadd_ctrl_t* ctrl );
modadd_status_e modadd_fixture_remove( modadd_fixture_t* node, modadd_ctrl_t* ctrl );

modadd_fixture_iter_t modadd_fixture_iter_first( modadd_fixture_iter_t head );
bool modadd_fixture_iter_done( modadd_fixture_iter_t iter );
modadd_fixture_iter_t modadd_fixture_iter_next( modadd_fixture_iter_t iter );
void modadd_fixture_foreach(modadd_fixture_iter_t head, void (*f)(modadd_fixture_iter_t iter, void*), void* args);

mp_obj_t addressable_fixture_get_dict( mp_obj_t fixture_obj, mp_int_t position );
mp_obj_t addressable_fixture_make_new( const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args );

// this is the actual C-structure for our new object
typedef struct _addressable_fixture_obj_t {
    mp_obj_base_t           base;       // base represents some basic information, like type
    modadd_fixture_t*       info;       // the fixture info
    modadd_layer_node_t*    layers;     // linked list of layers
} addressable_fixture_obj_t;




#endif // _MODADDRESSABLE_FIXTURE_H_