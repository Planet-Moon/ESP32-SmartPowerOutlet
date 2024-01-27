#include "MemoryWatcher.h"
#include "esp_log.h"
#include "esp_system.h"

constexpr char TAG[] = "MemoryWatcher";

MemoryWatcher::MemoryWatcher()
{
    // _free_heap_size_begin = esp_get_free_heap_size();
}

MemoryWatcher::~MemoryWatcher()
{
    // _free_heap_size_end = esp_get_free_heap_size();
    // const int32_t _diff = _free_heap_size_begin - _free_heap_size_end;
    // if(_diff > 0)
    // {
    //     ESP_LOGE(TAG, "MemoryWatcher leak of %ld bytes", _diff);
    // }
}
