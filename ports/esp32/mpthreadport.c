/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George on behalf of Pycom Ltd
 * Copyright (c) 2017 Pycom Limited
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

#include "stdio.h"

#include "py/mpconfig.h"
#include "py/mpstate.h"
#include "py/gc.h"
#include "py/mpthread.h"
#include "mpthreadport.h"

#include "esp_task.h"

#if MICROPY_PY_THREAD

#define MP_THREAD_MIN_STACK_SIZE                        (4 * 1024)
#define MP_THREAD_DEFAULT_STACK_SIZE                    (MP_THREAD_MIN_STACK_SIZE + 1024)
#define MP_THREAD_PRIORITY                              (ESP_TASK_PRIO_MIN + 1)

// this structure forms a linked list, one node per active thread
typedef struct _thread_t {
    TaskHandle_t id;        // system id of thread
    int ready;              // whether the thread is ready and running
    void *arg;              // thread Python args, a GC root pointer
    void *stack;            // pointer to the stack
    size_t stack_len;       // number of words in the stack
    struct _thread_t *next;
} thread_t;

// the mutex controls access to the linked list
typedef struct _ctx_thread_ctrl_t {
    mp_thread_mutex_t   mux;
    thread_t            entry0;
    thread_t*           thread;
} ctx_thread_ctrl_t;

void mp_thread_init(void *stack, uint32_t stack_len) {
    if( mp_active_context == NULL ){ return; }

    mp_thread_set_state(&mp_active_context_mirror.state->thread);
    ctx_thread_ctrl_t* thread_ctrl = (ctx_thread_ctrl_t*)mp_task_alloc( sizeof(ctx_thread_ctrl_t), mp_current_tID );
    if( thread_ctrl == NULL ){ return; }// todo: handle this error (no heap)
    mp_active_context->threadctrl = (void*)thread_ctrl;
    // create the first entry in the linked list of all threads
    thread_ctrl->thread = &(thread_ctrl->entry0);
    thread_ctrl->thread->id = (TaskHandle_t)mp_current_tID;
    thread_ctrl->thread->ready = 1;
    thread_ctrl->thread->arg = NULL;
    thread_ctrl->thread->stack = stack;
    thread_ctrl->thread->stack_len = stack_len;
    thread_ctrl->thread->next = NULL;
    mp_thread_mutex_init(&(thread_ctrl->mux));
}

void mp_thread_gc_others(void) {
    if( mp_active_context == NULL ){ return; }
    ctx_thread_ctrl_t* thread_ctrl = (ctx_thread_ctrl_t*)mp_active_context->threadctrl;
    if(thread_ctrl == NULL){ return; }

    mp_thread_mutex_lock(&(thread_ctrl->mux), 1);
    for (thread_t *th = (thread_ctrl->thread); th != NULL; th = th->next) {
        gc_collect_root((void**)&th, 1);
        gc_collect_root(&th->arg, 1); // probably not needed
        if (th->id == xTaskGetCurrentTaskHandle()) {
            continue;
        }
        if (!th->ready) {
            continue;
        }
        gc_collect_root(th->stack, th->stack_len); // probably not needed
    }
    mp_thread_mutex_unlock(&(thread_ctrl->mux));
}

mp_state_thread_t *mp_thread_get_state(void) {
    return pvTaskGetThreadLocalStoragePointer(NULL, 1);
}

void mp_thread_set_state(void *state) {
    vTaskSetThreadLocalStoragePointer(NULL, 1, state);
}

void mp_thread_start(void) {
    if( mp_active_context == NULL ){ return; }
    ctx_thread_ctrl_t* thread_ctrl = (ctx_thread_ctrl_t*)mp_active_context->threadctrl;
    if(thread_ctrl == NULL){ return; }

    mp_thread_mutex_lock(&(thread_ctrl->mux), 1);
    for (thread_t *th = (thread_ctrl->thread); th != NULL; th = th->next) {
        if (th->id == xTaskGetCurrentTaskHandle()) {
            th->ready = 1;
            break;
        }
    }
    mp_thread_mutex_unlock(&(thread_ctrl->mux));
}

STATIC void *(*ext_thread_entry)(void*) = NULL;

STATIC void freertos_entry(void *arg) {
    if (ext_thread_entry) {
        ext_thread_entry(arg);
    }
    vTaskDelete(NULL);
    for (;;);
}

void mp_thread_create_ex(void *(*entry)(void*), void *arg, size_t *stack_size, int priority, char *name) {
    // store thread entry function into a global variable so we can access it
    ext_thread_entry = entry;

    if (*stack_size == 0) {
        *stack_size = MP_THREAD_DEFAULT_STACK_SIZE; // default stack size
    } else if (*stack_size < MP_THREAD_MIN_STACK_SIZE) {
        *stack_size = MP_THREAD_MIN_STACK_SIZE; // minimum stack size
    }

    // Get thread control from this context
    if( mp_active_context == NULL ){ return; }
    ctx_thread_ctrl_t* thread_ctrl = (ctx_thread_ctrl_t*)mp_active_context->threadctrl;
    if(thread_ctrl == NULL){ return; }

    // Allocate linked-list node (must be outside thread_mutex lock)
    thread_t *th = m_new_obj(thread_t);

    mp_thread_mutex_lock(&(thread_ctrl->mux), 1);

    // create thread
    BaseType_t result = xTaskCreate(freertos_entry, name, *stack_size / sizeof(StackType_t), arg, priority, &th->id);
    if (result != pdPASS) {
        mp_thread_mutex_unlock(&(thread_ctrl->mux));
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "can't create thread"));
    }

    // adjust the stack_size to provide room to recover from hitting the limit
    *stack_size -= 1024;

    // add thread to linked list of all threads
    th->ready = 0;
    th->arg = arg;
    th->stack = pxTaskGetStackStart(th->id);
    th->stack_len = *stack_size / sizeof(StackType_t);
    th->next = (thread_ctrl->thread);
    (thread_ctrl->thread) = th;

    mp_thread_mutex_unlock(&(thread_ctrl->mux));
}

void mp_thread_create(void *(*entry)(void*), void *arg, size_t *stack_size) {
    mp_thread_create_ex(entry, arg, stack_size, MP_THREAD_PRIORITY, "mp_thread");
}

void mp_thread_finish(void) {    
    if( mp_active_context == NULL ){ return; }
    ctx_thread_ctrl_t* thread_ctrl = (ctx_thread_ctrl_t*)mp_active_context->threadctrl;
    if(thread_ctrl == NULL){ return; }

    mp_thread_mutex_lock(&(thread_ctrl->mux), 1);
    for (thread_t *th = (thread_ctrl->thread); th != NULL; th = th->next) {
        if (th->id == xTaskGetCurrentTaskHandle()) {
            th->ready = 0;
            break;
        }
    }
    mp_thread_mutex_unlock(&(thread_ctrl->mux));
}

void vPortCleanUpTCB(void *tcb) {
    thread_t *prev = NULL;
    mp_context_iter_t citer = NULL;
    for(citer = mp_context_iter_first(MP_ITER_FROM_CONTEXT_PTR(mp_context_head)); !mp_context_iter_done(citer); citer = mp_context_iter_next(citer)){
        ctx_thread_ctrl_t* thread_ctrl = (ctx_thread_ctrl_t*)(MP_CONTEXT_PTR_FROM_ITER(citer)->threadctrl);
        mp_thread_mutex_lock(&(thread_ctrl->mux), 1);
        for (thread_t *th = (thread_ctrl->thread); th != NULL; th = th->next) {
            // unlink the node from the list
            if ((void*)th->id == tcb) { // todo: make it more clear that tID, tcb, TaskHandle etc all mean the same thing
                if (prev != NULL) {
                    prev->next = th->next;
                } else {
                    // move the start pointer
                    (thread_ctrl->thread) = th->next;
                }
                // explicitly release all its memory
                m_del(thread_t, th, 1);
                mp_thread_mutex_unlock(&(thread_ctrl->mux));
                goto cleanup_complete;
            }
        }
        mp_thread_mutex_unlock(&(thread_ctrl->mux));
    }   
cleanup_complete:; // Need this odd ';' to avoid 'label at the end of compund statement' error
}

void mp_thread_mutex_init(mp_thread_mutex_t *mutex) {
    mutex->handle = xSemaphoreCreateMutexStatic(&mutex->buffer);
}

int mp_thread_mutex_lock(mp_thread_mutex_t *mutex, int wait) {
    return (pdTRUE == xSemaphoreTake(mutex->handle, wait ? portMAX_DELAY : 0));
}

void mp_thread_mutex_unlock(mp_thread_mutex_t *mutex) {
    xSemaphoreGive(mutex->handle);
}

void mp_thread_deinit(void) {
    if( mp_active_context == NULL ){ return; }
    ctx_thread_ctrl_t* thread_ctrl = (ctx_thread_ctrl_t*)mp_active_context->threadctrl;
    if(thread_ctrl == NULL){ return; }

    for (;;) {
        // Find a task to delete
        TaskHandle_t id = NULL;
        mp_thread_mutex_lock(&(thread_ctrl->mux), 1);
        for (thread_t *th = (thread_ctrl->thread); th != NULL; th = th->next) {
            // Don't delete the current task
            if (th->id != xTaskGetCurrentTaskHandle()) {
                id = th->id;
                break;
            }
        }
        mp_thread_mutex_unlock(&(thread_ctrl->mux));

        if (id == NULL) {
            // No tasks left to delete
            break;
        } else {
            // Call FreeRTOS to delete the task (it will call vPortCleanUpTCB)
            vTaskDelete(id);
        }
    }
}

#else

void vPortCleanUpTCB(void *tcb) {
}

#endif // MICROPY_PY_THREAD
