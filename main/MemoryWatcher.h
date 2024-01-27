#pragma once
#include <cstdint>

class MemoryWatcher{
public:
    MemoryWatcher();
    virtual ~MemoryWatcher();
private:
    uint32_t _free_heap_size_begin{0};
    uint32_t _free_heap_size_end{0};
}
