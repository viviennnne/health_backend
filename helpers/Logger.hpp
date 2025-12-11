#pragma once

#include <string>
#include <mutex>
#include <fstream>

namespace util {

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
    static void init(const std::string &filePath, LogLevel level = LogLevel::Info);
    static void shutdown();

    static void debug(const std::string &msg);
    static void info(const std::string &msg);
    static void warn(const std::string &msg);
    static void error(const std::string &msg);

private:
    static std::string timeStamp();
    static void log(LogLevel level, const std::string &msg);

    static std::mutex mtx_;
    static std::ofstream out_;    // optional file
    static LogLevel level_;
};

} // namespace util
