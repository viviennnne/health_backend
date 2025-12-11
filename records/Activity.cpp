#include "Activity.hpp"
#include "../external/json.hpp"

#include <unordered_map>
#include <algorithm>

using json = nlohmann::ordered_json;

// 假設 ActivityRecord：
// struct ActivityRecord {
//     std::string date;
//     int minutes;
//     std::string intensity;
//};

namespace {
    bool compareByDate(const ActivityRecord& a, const ActivityRecord& b) {
        return a.date < b.date;
    }
}

bool ActivityManager::addRecord(const std::string& userName,
                                const std::string& date,
                                int minutes,
                                const std::string& intensity) {
    auto& vec = data[userName];
    vec.push_back(ActivityRecord{date, minutes, intensity});
    std::sort(vec.begin(), vec.end(), compareByDate);
    return true;
}

bool ActivityManager::updateRecord(const std::string& userName,
                                   std::size_t index,
                                   const std::string& newDate,
                                   int newMinutes,
                                   const std::string& newIntensity) {
    auto it = data.find(userName);
    if (it == data.end()) return false;
    auto& vec = it->second;
    if (index >= vec.size()) return false;

    vec[index].date      = newDate;
    vec[index].minutes   = newMinutes;
    vec[index].intensity = newIntensity;
    std::sort(vec.begin(), vec.end(), compareByDate);
    return true;
}

bool ActivityManager::deleteRecord(const std::string& userName,
                                   std::size_t index) {
    auto it = data.find(userName);
    if (it == data.end()) return false;
    auto& vec = it->second;
    if (index >= vec.size()) return false;

    vec.erase(vec.begin() + static_cast<long>(index));
    return true;
}

std::vector<ActivityRecord> ActivityManager::getAll(const std::string& userName) const {
    auto it = data.find(userName);
    if (it == data.end()) return {};
    return it->second;
}

void ActivityManager::sortByDuration(const std::string& userName) {
    auto it = data.find(userName);
    if (it == data.end()) return;
    auto& vec = it->second;

    std::sort(vec.begin(), vec.end(),
              [](const ActivityRecord& a, const ActivityRecord& b) {
                  return a.minutes > b.minutes; // 由大到小
              });
}

// ===== JSON =====

json ActivityManager::toJson() const {
    json root = json::object();
    for (const auto& [user, vec] : data) {
        json arr = json::array();
        for (const auto& a : vec) {
            json ja;
            ja["date"]      = a.date;
            ja["minutes"]   = a.minutes;
            ja["intensity"] = a.intensity;
            arr.push_back(ja);
        }
        root[user] = arr;
    }
    return root;
}

void ActivityManager::fromJson(const json& j) {
    data.clear();
    if (!j.is_object()) return;

    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string user = it.key();
        const json& arr        = it.value();
        if (!arr.is_array()) continue;

        auto& vec = data[user];
        for (const auto& ja : arr) {
            ActivityRecord a;
            a.date      = ja.value("date", "");
            a.minutes   = ja.value("minutes", 0);
            a.intensity = ja.value("intensity", "");
            if (!a.date.empty()) {
                vec.push_back(a);
            }
        }
        std::sort(vec.begin(), vec.end(), compareByDate);
    }
}