#include "Water.hpp"
#include "../external/json.hpp"

#include <unordered_map>
#include <algorithm>

using json = nlohmann::ordered_json;

// 假設 WaterRecord 在 Water.hpp 是：
// struct WaterRecord {
//     std::string date;   // "YYYY-MM-DD"
//     double amountMl;
// };

namespace {
    // 讓日期排序：因為格式是 YYYY-MM-DD，字串排序就等於時間排序
    bool compareByDate(const WaterRecord& a, const WaterRecord& b) {
        return a.date < b.date;
    }
}

bool WaterManager::addRecord(const std::string& userName,
                             const std::string& date,
                             double amountMl) {
    auto& vec = data[userName];
    vec.push_back(WaterRecord{date, amountMl});
    std::sort(vec.begin(), vec.end(), compareByDate);
    return true;
}

bool WaterManager::updateRecord(const std::string& userName,
                                std::size_t index,
                                const std::string& newDate,
                                double newAmountMl) {
    auto it = data.find(userName);
    if (it == data.end()) return false;
    auto& vec = it->second;
    if (index >= vec.size()) return false;

    vec[index].date     = newDate;
    vec[index].amountMl = newAmountMl;
    std::sort(vec.begin(), vec.end(), compareByDate);
    return true;
}

bool WaterManager::deleteRecord(const std::string& userName,
                                std::size_t index) {
    auto it = data.find(userName);
    if (it == data.end()) return false;
    auto& vec = it->second;
    if (index >= vec.size()) return false;

    vec.erase(vec.begin() + static_cast<long>(index));
    return true;
}

std::vector<WaterRecord> WaterManager::getAll(const std::string& userName) const {
    auto it = data.find(userName);
    if (it == data.end()) return {};
    return it->second; // 已經是照日期排序好的
}

double WaterManager::getWeeklyAverage(const std::string& userName) const {
    auto it = data.find(userName);
    if (it == data.end() || it->second.empty()) return 0.0;

    const auto& vec = it->second;
    // 取「最後 7 筆」的平均
    std::size_t n = vec.size();
    std::size_t start = (n > 7) ? (n - 7) : 0;

    double sum = 0.0;
    for (std::size_t i = start; i < n; ++i) {
        sum += vec[i].amountMl;
    }
    std::size_t count = n - start;
    if (count == 0) return 0.0;
    return sum / static_cast<double>(count);
}

bool WaterManager::isEnoughForWeek(const std::string& userName,
                                   double dailyGoalMl) const {
    double avg = getWeeklyAverage(userName);
    return avg >= dailyGoalMl;
}

// ===== JSON =====

json WaterManager::toJson() const {
    json root = json::object();
    for (const auto& [user, vec] : data) {
        json arr = json::array();
        for (const auto& r : vec) {
            json jr;
            jr["date"]     = r.date;
            jr["amountMl"] = r.amountMl;
            arr.push_back(jr);
        }
        root[user] = arr;
    }
    return root;
}

void WaterManager::fromJson(const json& j) {
    data.clear();
    if (!j.is_object()) return;

    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string user = it.key();
        const json& arr        = it.value();
        if (!arr.is_array()) continue;

        auto& vec = data[user];
        for (const auto& jr : arr) {
            WaterRecord r;
            r.date     = jr.value("date", "");
            r.amountMl = jr.value("amountMl", 0.0);
            if (!r.date.empty()) {
                vec.push_back(r);
            }
        }
        std::sort(vec.begin(), vec.end(), compareByDate);
    }
}