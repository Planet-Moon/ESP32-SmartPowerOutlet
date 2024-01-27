#pragma once
#include <stdint.h>

class TimeMeasurement{
public:
    void start();
    void end();
    void reset();

    uint32_t duration();

private:
    uint32_t _start = 0;
    uint32_t _end = 0;

};
