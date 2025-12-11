#include "OtherCategory.hpp"
#include "../external/json.hpp"

#include <unordered_map>
#include <algorithm>

using json = nlohmann::ordered_json;

// 假設 OtherRecord：
// struct OtherRecord {
//     std::string date;
//     double value;
//     std::string note;
//};

namespace {
    bool compareByDate(const OtherRecord& a, const OtherRecord& b) {
        return a.date < b.date;
    }
}

bool OtherCategoryManager::addRecord(const std::string& userName,
                                     const std::string& categoryName,
                                     const std::string& date,
                                     double value,
                                     const std::string& note) {
    auto& catMap = data[userName];
    auto& vec    = catMap[categoryName];
    vec.push_back(OtherRecord{date, value, note});
    std::sort(vec.begin(), vec.end(), compareByDate);
    return true;
}

bool OtherCategoryManager::updateRecord(const std::string& userName,
                                        const std::string& categoryName,
                                        std::size_t index,
                                        const std::string& newDate,
                                        double newValue,
                                        const std::string& newNote) {
    auto itUser = data.find(userName);
    if (itUser == data.end()) return false;

    auto itCat = itUser->second.find(categoryName);
    if (itCat == itUser->second.end()) return false;

    auto& vec = itCat->second;
    if (index >= vec.size()) return false;

    vec[index].date  = newDate;
    vec[index].value = newValue;
    vec[index].note  = newNote;
    std::sort(vec.begin(), vec.end(), compareByDate);
    return true;
}

bool OtherCategoryManager::deleteRecord(const std::string& userName,
                                        const std::string& categoryName,
                                        std::size_t index) {
    auto itUser = data.find(userName);
    if (itUser == data.end()) return false;

    auto itCat = itUser->second.find(categoryName);
    if (itCat == itUser->second.end()) return false;

    auto& vec = itCat->second;
    if (index >= vec.size()) return false;

    vec.erase(vec.begin() + static_cast<long>(index));
    return true;
}

std::vector<std::string> OtherCategoryManager::getCategories(const std::string& userName) const {
    std::vector<std::string> result;
    auto itUser = data.find(userName);
    if (itUser == data.end()) return result;

    for (const auto& kv : itUser->second) {
        result.push_back(kv.first);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<OtherRecord> OtherCategoryManager::getRecords(const std::string& userName,
                                                          const std::string& categoryName) const {
    auto itUser = data.find(userName);
    if (itUser == data.end()) return {};

    auto itCat = itUser->second.find(categoryName);
    if (itCat == itUser->second.end()) return {};

    return itCat->second; // 已照日期排序
}

// ===== JSON =====

json OtherCategoryManager::toJson() const {
    json root = json::object();
    for (const auto& [user, catMap] : data) {
        json jUser = json::object();
        for (const auto& [cat, vec] : catMap) {
            json arr = json::array();
            for (const auto& r : vec) {
                json jr;
                jr["date"]  = r.date;
                jr["value"] = r.value;
                jr["note"]  = r.note;
                arr.push_back(jr);
            }
            jUser[cat] = arr;
        }
        root[user] = jUser;
    }
    return root;
}

void OtherCategoryManager::fromJson(const json& j) {
    data.clear();
    if (!j.is_object()) return;

    for (auto itUser = j.begin(); itUser != j.end(); ++itUser) {
        const std::string user = itUser.key();
        const json& jUser      = itUser.value();
        if (!jUser.is_object()) continue;

        auto& catMap = data[user];
        for (auto itCat = jUser.begin(); itCat != jUser.end(); ++itCat) {
            const std::string cat = itCat.key();
            const json& arr       = itCat.value();
            if (!arr.is_array()) continue;

            auto& vec = catMap[cat];
            for (const auto& jr : arr) {
                OtherRecord r;
                r.date  = jr.value("date", "");
                r.value = jr.value("value", 0.0);
                r.note  = jr.value("note", "");
                if (!r.date.empty()) {
                    vec.push_back(r);
                }
            }
            std::sort(vec.begin(), vec.end(), compareByDate);
        }
    }
}