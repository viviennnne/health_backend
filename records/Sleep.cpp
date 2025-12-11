#include "Sleep.hpp"
#include "../external/json.hpp"

#include <unordered_map>
#include <algorithm>

using json = nlohmann::ordered_json;

// 假設 SleepRecord：
// struct SleepRecord {
//     std::string date;
//     double hours;
// };

namespace {
    bool compareByDate(const SleepRecord& a, const SleepRecord& b) {
        return a.date < b.date;
    }
}

bool SleepManager::addRecord(const std::string& userName,
                             const std::string& date,
                             double hours) {
    auto& vec = data[userName];
    vec.push_back(SleepRecord{date, hours});
    std::sort(vec.begin(), vec.end(), compareByDate);
    return true;
}

bool SleepManager::updateRecord(const std::string& userName,
                                std::size_t index,
                                const std::string& newDate,
                                double newHours) {
    auto it = data.find(userName);
    if (it == data.end()) return false;
    auto& vec = it->second;
    if (index >= vec.size()) return false;

    vec[index].date  = newDate;
    vec[index].hours = newHours;
    std::sort(vec.begin(), vec.end(), compareByDate);
    return true;
}

bool SleepManager::deleteRecord(const std::string& userName,
                                std::size_t index) {
    auto it = data.find(userName);
    if (it == data.end()) return false;
    auto& vec = it->second;
    if (index >= vec.size()) return false;

    vec.erase(vec.begin() + static_cast<long>(index));
    return true;
}

std::vector<SleepRecord> SleepManager::getAll(const std::string& userName) const {
    auto it = data.find(userName);
    if (it == data.end()) return {};
    return it->second;
}

double SleepManager::getLastSleepHours(const std::string& userName) const {
    auto it = data.find(userName);
    if (it == data.end() || it->second.empty()) return 0.0;
    // 因為已排序，所以最後一個是最近日期
    return it->second.back().hours;
}

bool SleepManager::isSleepEnough(const std::string& userName,
                                 double minHours) const {
    double last = getLastSleepHours(userName);
    return last >= minHours;
}

// ===== JSON =====

json SleepManager::toJson() const {
    json root = json::object();
    for (const auto& [user, vec] : data) {
        json arr = json::array();
        for (const auto& r : vec) {
            json jr;
            jr["date"]  = r.date;
            jr["hours"] = r.hours;
            arr.push_back(jr);
        }
        root[user] = arr;
    }
    return root;
}

void SleepManager::fromJson(const json& j) {
    data.clear();
    if (!j.is_object()) return;

    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string user = it.key();
        const json& arr        = it.value();
        if (!arr.is_array()) continue;

        auto& vec = data[user];
        for (const auto& jr : arr) {
            SleepRecord r;
            r.date  = jr.value("date", "");
            r.hours = jr.value("hours", 0.0);
            if (!r.date.empty()) {
                vec.push_back(r);
            }
        }
        std::sort(vec.begin(), vec.end(), compareByDate);
    }
}