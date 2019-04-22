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

#ifndef MICROPY_INCLUDED_PY_MPSTATE_SPIRAM_H
#define MICROPY_INCLUDED_PY_MPSTATE_SPIRAM_H

#include "py/mpstate.h"

#include "esp_spiram.h" // temporary - there should not be any port-specific direct references within the core.... (keep it local to the port!)

#include "string.h"


// void* mp_context_dynmem_alloc_heap_caps( size_t size, mp_context_node_t* context, uint32_t caps ); // allocate memory in a context using the cpability-based allocate in the esp32
void* mp_task_alloc_heap_caps( size_t size, uint32_t tID, uint32_t caps );

#endif // MICROPY_INCLUDED_PY_MPSTATE_SPIRAM_H