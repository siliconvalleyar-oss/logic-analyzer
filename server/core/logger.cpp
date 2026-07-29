//==============================================================================
// logger.cpp
// Implementacion del sistema de logging
// Licencia: MIT
//==============================================================================

#include "logger.h"
#include <cstdarg>
#include <ctime>
#include <cstring>
#include <iostream>

FILE* Logger::file_ = nullptr;
LogLevel Logger::min_level_ = LOG_INFO;

void Logger::init(const std::string& filepath, LogLevel min_level) {
    min_level_ = min_level;
    if (!filepath.empty()) {
        file_ = fopen(filepath.c_str(), "a");
        if (file_) {
            setvbuf(file_, nullptr, _IONBF, 0);
        } else {
            std::cerr << "[Logger] Failed to open log file: "
                      << filepath << std::endl;
        }
    }
}

void Logger::shutdown() {
    if (file_) {
        fclose(file_);
        file_ = nullptr;
    }
}

void Logger::set_min_level(LogLevel level) {
    min_level_ = level;
}

const char* Logger::level_str(LogLevel l) {
    switch (l) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        case LOG_FATAL: return "FATAL";
        default:        return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const std::string& module,
                 const char* fmt, ...) {
    if (level < min_level_) return;

    // Timestamp
    char ts_buf[64];
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%d %H:%M:%S", &tm);

    // Mensaje formateado
    char msg_buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    // Linea completa
    char line[4224];
    snprintf(line, sizeof(line), "[%s] [%s] [%s] %s\n",
             ts_buf, level_str(level), module.c_str(), msg_buf);

    // Escribir a archivo
    if (file_) {
        fputs(line, file_);
    }

    // Escribir a stderr
    std::cerr << line;
}
