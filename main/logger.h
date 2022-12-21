#pragma once
#include <memory>
#include <array>
#include <string>
#include <apps/esp_sntp.h>
#include "helpers.h"

class Logger{
private:
    Logger();
    ~Logger();

    struct LogEntry{
        timeval timestamp;
        std::string category;
        std::string message;
        unique_ptr_json to_json() const;
    };
    std::array<LogEntry, 50> data;

    void rotate_data();

    int _log_counter = 0;
    const int _log_counter_max = 50;

public:
    Logger(const Logger&) = delete;
    Logger& operator = (const Logger&) = delete;

    void addLog(const std::string& category, const std::string& message);

    unique_ptr_json getLog() const;

    static Logger& getInstance(){
        static Logger instance;        // (1)
        return instance;
    }
};
