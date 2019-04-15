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

#include "modaddressable_timers.h"


modadd_status_e modadd_timer_start( modadd_ctrl_t* ctrl ){
    if( ctrl == NULL ){ return MODADD_STAT_ERR; }
    modadd_output_timer_t* timer = &(ctrl->timer);

    esp_timer_create_args_t timer_args;
    timer_args.callback = timer->callback;
    timer_args.arg = ctrl; // provide the control structure in the callback

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &(timer->timer_handle)));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer->timer_handle, timer->period));

    return MODADD_STAT_OK;
}