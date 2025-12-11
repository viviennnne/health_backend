#ifndef SLEEP_HPP
#define SLEEP_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include "../external/json.hpp"   // 使用 nlohmann::json

struct SleepRecord {
    std::string date; // "YYYY-MM-DD"
    double hours;     // 睡眠小時數
};

class SleepManager {
private:
    std::unordered_map<std::string, std::vector<SleepRecord>> data;

public:
    bool addRecord(const std::string& userName,
                   const std::string& date,
                   double hours);

    bool updateRecord(const std::string& userName,
                      std::size_t index,
                      const std::string& newDate,
                      double newHours);

    bool deleteRecord(const std::string& userName,
                      std::size_t index);

    std::vector<SleepRecord> getAll(const std::string& userName) const;

    double getLastSleepHours(const std::string& userName) const;

    bool isSleepEnough(const std::string& userName,
                       double minHours) const;

    // JSON 匯出 / 匯入
    nlohmann::ordered_json toJson() const;
    void fromJson(const nlohmann::ordered_json& j);
};

#endif