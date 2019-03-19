/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/mpstate.h"

#if MICROPY_DYNAMIC_COMPILER
mp_dynamic_compiler_t mp_dynamic_compiler = {0};
#endif

void mp_context_switch(mp_context_node_t* node){
    mp_active_context = *(node);
    mp_active_dict_main = MP_STATE_VM(dict_main);
    #if MICROPY_PY_SYS
    mp_active_loaded_modules_dict = MP_STATE_VM(mp_loaded_modules_dict);
    // mp_active_sys_path_obj; // ToDo: determine source for these variables
    // mp_active_sys_argv_obj; // ToDo: determine source for these variables
    #endif // MICROPY_PY_SYS
}

mp_state_ctx_t _hidden_mp_state_ctx;

#define MP_DEFAULT_CONTEXT_ID 0 // ToDo: move this into proper configuration files
mp_context_node_t mp_default_context = {
    .id = MP_DEFAULT_CONTEXT_ID,
    .state = &_hidden_mp_state_ctx,
    .next = NULL,
};

mp_context_node_t mp_active_context;
mp_context_node_t* mp_context_head = &mp_default_context;