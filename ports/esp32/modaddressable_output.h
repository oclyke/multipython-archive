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

#ifndef _MODADDRESSABLE_OUTPUT_H_
#define _MODADDRESSABLE_OUTPUT_H_

#include "modaddressable.h"

modadd_status_e modadd_output_initialize( modadd_ctrl_t* ctrl );

modadd_status_e mach1_output_init_apa102_hw( modadd_ctrl_t* ctrl );
modadd_status_e mach1_output_init_apa102_sw( modadd_ctrl_t* ctrl );

IRAM_ATTR void mach1_output_apa102_hw(void* arg); // arg should be an output pointer, passed from timer function
IRAM_ATTR void mach1_output_apa102_sw(void* arg);

#endif // _MODADDRESSABLE_OUTPUT_H_