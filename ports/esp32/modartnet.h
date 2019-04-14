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

#ifndef _MODARTNET_H_
#define _MODARTNET_H_

#include "py/mpstate.h"

#define ARTNET_MALLOC(size) malloc(size)
#define ARTNET_FREE(ptr) free(ptr)

typedef void (*artnet_callback_t)( void );
typedef void (*artnet_callback_args_t)( void* args );

typedef enum{
    AN_CLBK_IS_PYTHON = 0x01,

}artnet_callback_flags_e;

typedef struct _artnet_callback_node_t{
    artnet_callback_args_t          c_callback;
    mp_obj_t                        p_callback;
    mp_context_node_t*              p_context;
    void*                           args;
    struct _artnet_callback_node_t* next;
}artnet_callback_node_t;





#define ARTNET_ID_LEN 8
#define ARTNET_MAX_DATA_LEN 512
typedef struct _artnet_packet_t{
    uint8_t ID[ARTNET_ID_LEN];
    union{
        uint16_t OpCodeLittleEndian;
        struct{
            uint8_t OpCodeLo;
            uint8_t OpCodeHi;
        }OpCodeBytes;
    }OpCode;
    union{
        uint16_t ProtVerBigEndian;
        struct{
            uint8_t ProtVerHi;
            uint8_t ProtVerLo;
        }ProtVerBytes;
    }ProtVer;
    uint8_t Sequence;
    uint8_t Physical;
    uint8_t SubUni;
    uint8_t Net;
    union{
        uint16_t LengthBigEndian;
        struct{
            uint8_t LengthHi;
            uint8_t LengthLo;
        }LengthBytes;
    }Length;
    uint8_t Data[ARTNET_MAX_DATA_LEN];
}artnet_packet_t;


extern volatile artnet_packet_t artnet_packet;

mp_obj_t artnet_start( mp_obj_t interface );
mp_obj_t artnet_stop( void );

// callback linked list
artnet_callback_node_t* artnet_append_callback_node( void );
int artnet_remove_callback_node( artnet_callback_node_t* node );

int8_t artnet_add_callback_c_args( artnet_callback_args_t cb, void* args );
int8_t artnet_add_callback_c( artnet_callback_t cb );



#endif // _MODARTNET_H_