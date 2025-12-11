#include "Logger.hpp"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <iomanip>

namespace util {

std::mutex Logger::mtx_;
std::ofstream Logger::out_;
LogLevel Logger::level_ = LogLevel::Info;

void Logger::init(const std::string &filePath, LogLevel level) {
    std::lock_guard<std::mutex> lk(mtx_);
    level_ = level;
    if (!filePath.empty()) {
        try {
            std::filesystem::path p(filePath);
            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        } catch (...) {
            // ignore failures creating directory
        }
        out_.open(filePath, std::ios::app);
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (out_.is_open()) {
        out_.flush();
        out_.close();
    }
}

std::string Logger::timeStamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto itt = system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&itt), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void Logger::log(LogLevel level, const std::string &msg) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (level < level_) return;
    std::string lvl;
    switch (level) {
        case LogLevel::Debug: lvl = "DEBUG"; break;
        case LogLevel::Info: lvl = "INFO"; break;
        case LogLevel::Warning: lvl = "WARN"; break;
        case LogLevel::Error: lvl = "ERROR"; break;
    }
    std::ostringstream oss;
    oss << "[" << timeStamp() << "] [" << lvl << "] " << msg << "\n";
    const std::string outStr = oss.str();
    // write to stdout
    std::fwrite(outStr.data(), 1, outStr.size(), stdout);
    // flush to file if open
    if (out_.is_open()) {
        out_ << outStr;
        out_.flush();
    }
}

void Logger::debug(const std::string &msg) { log(LogLevel::Debug, msg); }
void Logger::info(const std::string &msg)  { log(LogLevel::Info, msg); }
void Logger::warn(const std::string &msg)  { log(LogLevel::Warning, msg); }
void Logger::error(const std::string &msg) { log(LogLevel::Error, msg); }

} // namespace util
