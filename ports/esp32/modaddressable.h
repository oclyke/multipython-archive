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

With the "addressable" module you can manage addressable LED outputs. Each output has both
characteristics about the output such as the protocol and hardware information, as well as 
a pointer to a linked list of LED fixtures. This allows one to easily add/rearrange the 
output of LED data to match the current hardware. 

For efficient out-transfers of LED data all the data for every fixture in the linked list
is allocated as a single block. To avoid memory fragmentation as much as possible memory for
and output will only be allocated:
    a) manually
    b) when starting ouput with a new size
    c) if fixture operations change the memory size while output is on


*/

#ifndef _MODADDRESSABLE_H_
#define _MODADDRESSABLE_H_

#include <stdint.h>
#include <string.h>

#include "py/obj.h"
#include "py/mperrno.h"
#include "py/runtime.h"

#include "modaddressable_types.h"
#include "modaddressable_protocols.h"
#include "modaddressable_timers.h"
#include "modaddressable_fixture.h"
#include "modaddressable_output.h"
#include "modaddressable_controllers.h"

#define MODADD_MALLOC(size) malloc(size)
#define MODADD_MALLOC_DMA(size) heap_caps_malloc( size, MALLOC_CAP_DMA );
#define MODADD_FREE(ptr) free(ptr)


void modadd_ctrl_recompute_fixtures( modadd_ctrl_t* ctrl );


#endif // _MODADDRESSABLE_H_