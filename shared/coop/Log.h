// Minimal thread-safe logger. Writes to stdout (server) and/or a file (client).
#pragma once

#include <string>
#include <mutex>
#include <cstdarg>

namespace coop {

enum class LogLevel : int { Debug = 0, Info = 1, Warn = 2, Error = 3 };

class Log {
public:
    // Configure once at startup. Empty filePath disables file logging.
    static void Init(const std::string& filePath, LogLevel minLevel);
    static void Shutdown();

    static void Debug(const char* fmt, ...);
    static void Info(const char* fmt, ...);
    static void Warn(const char* fmt, ...);
    static void Error(const char* fmt, ...);

private:
    static void Write(LogLevel level, const char* fmt, va_list args);
};

} // namespace coop
