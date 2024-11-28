#include "Logger.h"
#include <spdlog/spdlog.h>

void Logger::init() {
    spdlog::set_pattern("[%T] [%^%l%$] %v");
    spdlog::info("Logger initialized");
}

void Logger::info(const std::string& message) {
    spdlog::info(message);
}

void Logger::error(const std::string& message) {
    spdlog::error(message);
}
