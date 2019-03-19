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
#ifndef _MODMACH1_H_
#define _MODMACH1_H_

#include "stdint.h"

typedef struct _mach1_firmware_info_t {
    uint8_t major;
    uint8_t minor; 
    uint8_t patch;
}mach1_firmware_info_t;

#define MACH1_DEVICE_NAME_MAX_LEN  (32)                         // SAMPLE_DEVICE_NAME + 6 MAC address numbers (2 hex chars each, max) + 5 separating .s + NULL
extern char     mach1_device_name[MACH1_DEVICE_NAME_MAX_LEN];   // Will fill this at run-time with snprintf("Mach1 LED %02X.%02X.%02X.%02X.%02X.%02X", chip_id[0], chip_id[1], chip_id[2], chip_id[3], chip_id[4], chip_id[5] );
extern uint8_t  mach1_chip_id[6];                               // Set this at runtime with esp_efuse_mac_get_default(chip_id)




#endif  // _MODMACH1_H_