#include "TimeMeasurement.h"
#include <hal/cpu_hal.h>

void TimeMeasurement::start(){
    _start = cpu_hal_get_cycle_count();
    _end = 0;
}

void TimeMeasurement::end(){
    if(_start > 0)
        _end = cpu_hal_get_cycle_count();
}

void TimeMeasurement::reset(){
    _start = 0;
    _end = 0;
}

uint32_t TimeMeasurement::duration(){
    if(_end > 0 && _start > 0)
        return _end - _start;
    return 0;
}
