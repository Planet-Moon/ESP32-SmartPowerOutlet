#include "logger.h"
#include "esp_log.h"

constexpr char LOG_LOGGER[] = "Logger";
constexpr char LOG_SNTP[] = "sntp_status";

Logger::Logger()
{
    int sntp_status = sntp_get_sync_status();
    ESP_LOGI(LOG_LOGGER, "sntp_status: %d", sntp_status);
    switch (sntp_status){
        case 0:
            ESP_LOGI(LOG_SNTP, "Reset");
            break;
        case 1:
            ESP_LOGI(LOG_SNTP, "Completed");
            break;
        case 2:
            ESP_LOGI(LOG_SNTP, "In progress");
            break;
        default:
            break;
    }
}

Logger::~Logger()
{

}

void rotate_data(){

}

void Logger::addLog(const std::string& category, const std::string& message)
{

    LogEntry entry;
    ESP_LOGI(LOG_LOGGER, "adding data: %s %s", category.c_str(), message.c_str());
    if(gettimeofday(&entry.timestamp, NULL) < 0){
        ESP_LOGE(LOG_LOGGER, "error reading time");
        return;
    }


    if(data.size() > 1)
        std::rotate(data.rbegin(), data.rbegin()+1, data.rend());

    entry.category = category;
    entry.message = message;

    data[0] = entry;
    ESP_LOGI(LOG_LOGGER, "added data");

    if(_log_counter < _log_counter_max)
        _log_counter++;
}

unique_ptr_json Logger::LogEntry::to_json() const
{
    auto json = unique_ptr_json(cJSON_CreateObject(), json_delete);
    cJSON_AddNumberToObject(json.get(), "seconds", timestamp.tv_sec);
    cJSON_AddNumberToObject(json.get(), "microseconds", timestamp.tv_usec);
    cJSON_AddStringToObject(json.get(), "category", category.c_str());
    cJSON_AddStringToObject(json.get(), "message", message.c_str());
    return json;
}

unique_ptr_json Logger::getLog() const
{
    auto json = unique_ptr_json(cJSON_CreateArray(), json_delete);
    for(size_t i = 0; i < _log_counter; ++i){
        cJSON* entry_json = data[i].to_json().release();
        cJSON_AddItemToArray(json.get(), entry_json);
    }
    return json;
}
