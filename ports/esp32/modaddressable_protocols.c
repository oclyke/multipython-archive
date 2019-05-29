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

#include "modaddressable_protocols.h"



const uint8_t apa102_or_mask[] = { 0xE0, 0x00, 0x00, 0x00 };
const modadd_color_ind_e apa102_indices[] = { MODADD_A_INDEX, MODADD_B_INDEX, MODADD_G_INDEX, MODADD_R_INDEX };
const uint8_t apa102_leading[] = { 0x00, 0x00, 0x00, 0x00 };
const modadd_protocol_t modadd_protocol_apa102 = {
    .bpl = 4,
    .or_mask = apa102_or_mask,
    .indices = apa102_indices,
    .num_leading_const = 4,
    .num_leading_rate = 0,
    // .leading = apa102_leading,
    .num_trailing_const = 0, 
    .num_trailing_rate = 16,
    // .trailing = NULL,
    .brightness_rightshifts = 3,
};

const uint8_t ws2812_or_mask[] = { 0x00, 0x00, 0x00 };
const modadd_color_ind_e ws2812_indices[] = { MODADD_G_INDEX, MODADD_R_INDEX, MODADD_B_INDEX };
const modadd_protocol_t modadd_protocol_ws2812 = {
    .bpl = 3,
    .or_mask = ws2812_or_mask,
    .indices = ws2812_indices,
    .num_leading_const = 5,   // ws2812 uses a reset code of 50 us, that's 40 bits at 1.25 us/bit, or 5 bytes. But the bytes should be all low, rather than the '0' code which has high and low portions
    .num_leading_rate = 0,
    // .leading = NULL,
    .num_trailing_const = 0, 
    .num_trailing_rate = 0,
    // .trailing = NULL,
    .brightness_rightshifts = 0,
};

const modadd_protocol_t* modadd_protocols[MODADD_PROTOCOLS_NUM] = {
    &modadd_protocol_apa102,
    &modadd_protocol_ws2812,
};