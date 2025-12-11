#ifndef WATER_HPP
#define WATER_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include "../external/json.hpp"   // 使用 nlohmann::json

struct WaterRecord {
    std::string date;   // "YYYY-MM-DD"
    double amountMl;    // 當天飲水量
};

class WaterManager {
private:
    std::unordered_map<std::string, std::vector<WaterRecord>> data;

public:
    bool addRecord(const std::string& userName,
                   const std::string& date,
                   double amountMl);

    bool updateRecord(const std::string& userName,
                      std::size_t index,
                      const std::string& newDate,
                      double newAmountMl);

    bool deleteRecord(const std::string& userName, std::size_t index);

    std::vector<WaterRecord> getAll(const std::string& userName) const;

    // 一週平均：取 user 最後最多 7 筆紀錄算平均
    double getWeeklyAverage(const std::string& userName) const;

    bool isEnoughForWeek(const std::string& userName,
                         double dailyGoalMl) const;

    // JSON 匯出 / 匯入
    nlohmann::ordered_json toJson() const;
    void fromJson(const nlohmann::ordered_json& j);
};

#endif