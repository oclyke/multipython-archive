/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_task.h"
#include "soc/cpu.h"
#include "esp_log.h"
#include "esp_spiram.h"

#include "py/stackctrl.h"
#include "py/nlr.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "lib/mp-readline/readline.h"
#include "lib/utils/pyexec.h"
#include "uart.h"
#include "modmachine.h"
#include "modnetwork.h"
#include "mpthreadport.h"

#include "mpstate_spiram.h"
#include "modmach1.h"

// MicroPython runs as a task under FreeRTOS
#define MP_TASK_PRIORITY        (ESP_TASK_PRIO_MIN + 1)
#define MP_TASK_STACK_SIZE      (16 * 1024)
#define MP_TASK_STACK_LEN       (MP_TASK_STACK_SIZE / sizeof(StackType_t))

int vprintf_null(const char *format, va_list ap) {
    // do nothing: this is used as a log target during raw repl mode
    return 0;
}

void mp_task(void *pvParameter) {
    // Main task has special treatment b/c the state is not dynamically allocated
    // Normally all that is required is to call mp_task_register with the ID and args
    uint32_t mp_task_id = mp_current_tIDs[MICROPY_GET_CORE_INDEX];
    mp_context_head->id = mp_task_id;
    mp_context_switch(mp_context_head);

    volatile uint32_t sp = (uint32_t)get_sp();
    #if MICROPY_PY_THREAD
    mp_thread_init(pxTaskGetStackStart(NULL), MP_TASK_STACK_LEN);
    #endif
    uart_init();

    size_t mp_task_heap_size;
    void *mp_task_heap = NULL;
    #if CONFIG_SPIRAM_SUPPORT
    switch (esp_spiram_get_chip_size()) {
        case ESP_SPIRAM_SIZE_16MBITS:
            // mp_task_heap_size = 2 * 1024 * 1024;
            mp_task_heap_size = 256 * 1024;
            break;
        case ESP_SPIRAM_SIZE_32MBITS:
        case ESP_SPIRAM_SIZE_64MBITS:
            // mp_task_heap_size = 4 * 1024 * 1024;
            mp_task_heap_size = 256 * 1024;
            break;
        default:
            // No SPIRAM, fallback to normal allocation
            // mp_task_heap_size = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
            // mp_task_heap = malloc(mp_task_heap_size); // todo: make heap size configurable or "extendable"
            mp_task_heap_size = 64000;
            break;
    }
    mp_task_heap = mp_task_alloc_heap_caps( mp_task_heap_size, mp_current_tIDs[MICROPY_GET_CORE_INDEX], MALLOC_CAP_SPIRAM ); // todo: make heap size configurable or "extendable"
    #else
    // Allocate the uPy heap using malloc and get the largest available region
    // size_t mp_task_heap_size = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    mp_task_heap_size = 64000;
    mp_task_heap = mp_task_alloc( mp_task_heap_size, mp_current_tIDs[MICROPY_GET_CORE_INDEX] );
    #endif
    
    if( mp_task_heap == NULL ){
        printf("Could not allocate memory for task. aborting\n");
        while(1){
            printf("You oughtaa fix this... are you trying to use SPIRAM? have you lowered the mp_task_heap_size value?\n");
            vTaskDelay(1000 / portTICK_RATE_MS );
        }
    }


soft_reset:
    // initialise the stack pointer for the main thread
    mp_stack_set_top((void *)sp);
    mp_stack_set_limit(MP_TASK_STACK_SIZE - 1024);
    gc_init(mp_task_heap, mp_task_heap + mp_task_heap_size);
    mp_init();
    mp_obj_list_init(mp_sys_path, 0);
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_));
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_lib));
    mp_obj_list_init(mp_sys_argv, 0);
    mp_context_refresh();
    readline_init0();

    // todo: add hook for ports to stop all other running contexts when rebooting

    // initialise peripherals
    machine_pins_init();

    // run boot-up scripts
    pyexec_frozen_module("_boot.py");
    pyexec_file("boot.py");
    if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL) {
        pyexec_file("main.py");
    }

    for (;;) {
        if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
            vprintf_like_t vprintf_log = esp_log_set_vprintf(vprintf_null);
            if (pyexec_raw_repl() != 0) {
                break;
            }
            esp_log_set_vprintf(vprintf_log);
        } else {
            if (pyexec_friendly_repl() != 0) {
                break;
            }
        }
        // todo: make sure REPL task does not starve IDLE0 task (b/c IDLE0 task cleans up memory from dead tasks)
    }

    machine_timer_deinit_all();

    #if MICROPY_PY_THREAD
    mp_thread_deinit();
    #endif

    gc_sweep_all();

    mp_hal_stdout_tx_str("MPY: soft reboot\r\n");

    // deinitialise peripherals
    machine_pins_deinit();
    usocket_events_deinit();

    mp_deinit();
    fflush(stdout);
    goto soft_reset;
}

extern void ble_server_begin( void );
void app_main(void) {
    nvs_flash_init();

    esp_efuse_mac_get_default(mach1_chip_id);
    snprintf(mach1_device_name, MACH1_DEVICE_NAME_MAX_LEN, "Mach1_LED_%02X.%02X.%02X.%02X.%02X.%02X", mach1_chip_id[0], mach1_chip_id[1], mach1_chip_id[2], mach1_chip_id[3], mach1_chip_id[4], mach1_chip_id[5] );
    printf("Hello from %s\n", mach1_device_name);

    ble_server_begin();

    xTaskCreatePinnedToCore(mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY, &mp_main_task_handle, MICROPY_REPL_CORE );
}

void nlr_jump_fail(void *val) {
    printf("NLR jump failed, val=%p\n", val);
    esp_restart();
}

// modussl_mbedtls uses this function but it's not enabled in ESP IDF
void mbedtls_debug_set_threshold(int threshold) {
    (void)threshold;
}
