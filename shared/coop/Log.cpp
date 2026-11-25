#include "Log.h"

#include <cstdio>
#include <cstdarg>
#include <ctime>

namespace coop {

namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
LogLevel g_minLevel = LogLevel::Info;

const char* LevelTag(LogLevel l) {
    switch (l) {
        case LogLevel::Debug: return "DBG";
        case LogLevel::Info:  return "INF";
        case LogLevel::Warn:  return "WRN";
        case LogLevel::Error: return "ERR";
    }
    return "?";
}
} // namespace

void Log::Init(const std::string& filePath, LogLevel minLevel) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_minLevel = minLevel;
    if (!filePath.empty() && !g_file) {
        g_file = std::fopen(filePath.c_str(), "ab");
    }
}

void Log::Shutdown() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_file) { std::fclose(g_file); g_file = nullptr; }
}

void Log::Write(LogLevel level, const char* fmt, va_list args) {
    if (level < g_minLevel) return;

    char msg[1024];
    std::vsnprintf(msg, sizeof(msg), fmt, args);

    char timebuf[32];
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    std::strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tmv);

    std::lock_guard<std::mutex> lk(g_mutex);
    std::fprintf(stdout, "[%s %s] %s\n", timebuf, LevelTag(level), msg);
    std::fflush(stdout);
    if (g_file) {
        std::fprintf(g_file, "[%s %s] %s\n", timebuf, LevelTag(level), msg);
        std::fflush(g_file);
    }
}

void Log::Debug(const char* fmt, ...) { va_list a; va_start(a, fmt); Write(LogLevel::Debug, fmt, a); va_end(a); }
void Log::Info(const char* fmt, ...)  { va_list a; va_start(a, fmt); Write(LogLevel::Info,  fmt, a); va_end(a); }
void Log::Warn(const char* fmt, ...)  { va_list a; va_start(a, fmt); Write(LogLevel::Warn,  fmt, a); va_end(a); }
void Log::Error(const char* fmt, ...) { va_list a; va_start(a, fmt); Write(LogLevel::Error, fmt, a); va_end(a); }

} // namespace coop
