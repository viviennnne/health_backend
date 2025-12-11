#ifndef ACTIVITY_HPP
#define ACTIVITY_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include "../external/json.hpp"   // 使用 nlohmann::json

struct ActivityRecord {
    std::string date;      // "YYYY-MM-DD"
    int         minutes;   // 活動時間（分鐘）
    std::string intensity; // "low" / "moderate" / "high"
};

class ActivityManager {
private:
    std::unordered_map<std::string, std::vector<ActivityRecord>> data;

public:
    bool addRecord(const std::string& userName,
                   const std::string& date,
                   int minutes,
                   const std::string& intensity);

    bool updateRecord(const std::string& userName,
                      std::size_t index,
                      const std::string& newDate,
                      int newMinutes,
                      const std::string& newIntensity);

    bool deleteRecord(const std::string& userName,
                      std::size_t index);

    std::vector<ActivityRecord> getAll(const std::string& userName) const;

    void sortByDuration(const std::string& userName);

    // JSON 匯出 / 匯入
    nlohmann::ordered_json toJson() const;
    void fromJson(const nlohmann::ordered_json& j);
};

#endif