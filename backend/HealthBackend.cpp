#include "HealthBackend.hpp"

#include <fstream>
#include <random>
#include <iostream>

// ----------------------
// 建構 / 解構：處理載入 / 儲存
// ----------------------

HealthBackend::HealthBackend()
    : storagePath("data/storage.json") {
    loadFromFile();
}

HealthBackend::~HealthBackend() {
    try {
        saveToFile();
    } catch (...) {
        // 不讓 destructor 拋例外
    }
}

// ----------------------
// Helper：產生 token
// ----------------------

std::string HealthBackend::generateToken() const {
    static const char chars[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    static constexpr std::size_t N = sizeof(chars) - 1;

    std::random_device rd;
    std::mt19937_64 rng(rd());
    std::uniform_int_distribution<std::size_t> dist(0, N - 1);

    std::string token;
    token.reserve(32);
    for (int i = 0; i < 32; ++i) {
        token.push_back(chars[dist(rng)]);
    }
    return token;
}

// ----------------------
// 檔案 I/O
// ----------------------

void HealthBackend::loadFromFile() {
    std::ifstream in(storagePath);
    if (!in) {
        // 檔案不存在 → 視為空資料庫
        return;
    }

    json j;
    try {
        in >> j;
    } catch (...) {
        std::cerr << "Failed to parse " << storagePath << ", starting empty.\n";
        return;
    }

    if (!j.contains("users") || !j["users"].is_array()) {
        return;
    }

    for (const auto& ju : j["users"]) {
        if (!ju.contains("name")) continue;
        std::string name = ju.value("name", "");

        UserData data;
        data.profile.id       = ju.value("id", name);
        data.profile.name     = name;
        data.profile.age      = ju.value("age", 0);
        data.profile.weightKg = ju.value("weightKg", 0.0);
        data.profile.heightM  = ju.value("heightM", 0.0);
        data.profile.gender   = ju.value("gender", std::string("other"));

        data.password         = ju.value("password", std::string(""));

        // Waters
        if (ju.contains("waters") && ju["waters"].is_array()) {
            for (const auto& jw : ju["waters"]) {
                WaterRecord w;
                w.datetime = jw.value("datetime", std::string(""));
                w.amountMl = jw.value("amountMl", 0.0);
                data.waters.push_back(w);
            }
        }

        // Sleeps
        if (ju.contains("sleeps") && ju["sleeps"].is_array()) {
            for (const auto& js : ju["sleeps"]) {
                SleepRecord s;
                s.datetime = js.value("datetime", std::string(""));
                s.hours    = js.value("hours", 0.0);
                data.sleeps.push_back(s);
            }
        }

        // Activities
        if (ju.contains("activities") && ju["activities"].is_array()) {
            for (const auto& ja : ju["activities"]) {
                ActivityRecord a;
                a.datetime  = ja.value("datetime", std::string(""));
                a.minutes   = ja.value("minutes", 0);
                a.intensity = ja.value("intensity", std::string(""));
                data.activities.push_back(a);
            }
        }

        // Categories
        if (ju.contains("categories") && ju["categories"].is_object()) {
            for (auto it = ju["categories"].begin();
                 it != ju["categories"].end(); ++it) {
                const std::string catName = it.key();
                const auto& arr = it.value();
                if (!arr.is_array()) continue;

                std::vector<CategoryItem> items;
                for (const auto& ji : arr) {
                    CategoryItem item;
                    item.datetime = ji.value("datetime", std::string(""));
                    item.note     = ji.value("note", std::string(""));
                    item.value    = ji.value("value", 0.0);
                    items.push_back(item);
                }
                data.categories[catName] = std::move(items);
            }
        }

        usersByName[name] = std::move(data);
    }
}

void HealthBackend::saveToFile() const {
    json j;
    j["users"] = json::array();

    for (const auto& [name, data] : usersByName) {
        json ju;
        ju["id"]       = data.profile.id;
        ju["name"]     = data.profile.name;
        ju["age"]      = data.profile.age;
        ju["weightKg"] = data.profile.weightKg;
        ju["heightM"]  = data.profile.heightM;
        ju["gender"]   = data.profile.gender;

        ju["password"] = data.password;

        // Waters
        ju["waters"] = json::array();
        for (const auto& w : data.waters) {
            json jw;
            jw["datetime"] = w.datetime;
            jw["amountMl"] = w.amountMl;
            ju["waters"].push_back(jw);
        }

        // Sleeps
        ju["sleeps"] = json::array();
        for (const auto& s : data.sleeps) {
            json js;
            js["datetime"] = s.datetime;
            js["hours"]    = s.hours;
            ju["sleeps"].push_back(js);
        }

        // Activities
        ju["activities"] = json::array();
        for (const auto& a : data.activities) {
            json ja;
            ja["datetime"]  = a.datetime;
            ja["minutes"]   = a.minutes;
            ja["intensity"] = a.intensity;
            ju["activities"].push_back(ja);
        }

        // Categories
        ju["categories"] = json::object();
        for (const auto& [catName, items] : data.categories) {
            json arr = json::array();
            for (const auto& item : items) {
                json ji;
                ji["datetime"] = item.datetime;
                ji["note"]     = item.note;
                ji["value"]    = item.value;
                arr.push_back(ji);
            }
            ju["categories"][catName] = arr;
        }

        j["users"].push_back(ju);
    }

    std::ofstream out(storagePath);
    if (!out) {
        std::cerr << "Failed to open " << storagePath << " for writing.\n";
        return;
    }
    out << j.dump(2);
}

// ----------------------
// Token → UserData
// ----------------------

HealthBackend::UserData* HealthBackend::getUserByToken(const std::string& token) {
    auto itTok = tokenToName.find(token);
    if (itTok == tokenToName.end()) return nullptr;
    auto itUser = usersByName.find(itTok->second);
    if (itUser == usersByName.end()) return nullptr;
    return &itUser->second;
}

const HealthBackend::UserData* HealthBackend::getUserByToken(const std::string& token) const {
    auto itTok = tokenToName.find(token);
    if (itTok == tokenToName.end()) return nullptr;
    auto itUser = usersByName.find(itTok->second);
    if (itUser == usersByName.end()) return nullptr;
    return &itUser->second;
}

bool HealthBackend::hasUserForToken(const std::string& token) const {
    return getUserByToken(token) != nullptr;
}

// ----------------------
// User / Auth
// ----------------------

bool HealthBackend::registerUser(const std::string& name,
                                 int                age,
                                 double             weightKg,
                                 double             heightM,
                                 const std::string& password,
                                 const std::string& gender) {
    if (name.empty() || password.empty()) return false;
    if (age <= 0 || weightKg <= 0.0 || heightM <= 0.0) return false;

    if (usersByName.find(name) != usersByName.end()) {
        // User already exists
        return false;
    }

    UserData data;
    data.profile.id       = name; // 簡單用 name 當 id
    data.profile.name     = name;
    data.profile.age      = age;
    data.profile.weightKg = weightKg;
    data.profile.heightM  = heightM;
    data.profile.gender   = gender;
    data.password         = password;

    usersByName[name] = std::move(data);
    saveToFile();
    return true;
}

std::string HealthBackend::login(const std::string& name,
                                 const std::string& password) {
    auto it = usersByName.find(name);
    if (it == usersByName.end()) {
        return "INVALID";
    }
    if (it->second.password != password) {
        return "INVALID";
    }

    // 產生新的 token
    std::string token = generateToken();
    tokenToName[token] = name;
    return token;
}

bool HealthBackend::getUserProfile(const std::string& token,
                                   UserProfile&       outProfile) const {
    const UserData* user = getUserByToken(token);
    if (!user) return false;
    outProfile = user->profile;
    return true;
}

double HealthBackend::getBMI(const std::string& token) const {
    const UserData* user = getUserByToken(token);
    if (!user) return 0.0;
    if (user->profile.heightM <= 0.0) return 0.0;
    if (user->profile.weightKg <= 0.0) return 0.0;

    return user->profile.weightKg / (user->profile.heightM * user->profile.heightM);
}

// ----------------------
// Waters
// ----------------------

bool HealthBackend::addWater(const std::string& token,
                             const std::string& datetime,
                             double             amountMl) {
    if (amountMl <= 0.0) return false;
    UserData* user = getUserByToken(token);
    if (!user) return false;

    WaterRecord w;
    w.datetime = datetime;
    w.amountMl = amountMl;
    user->waters.push_back(w);
    saveToFile();
    return true;
}

std::vector<WaterRecord> HealthBackend::getAllWater(const std::string& token) const {
    const UserData* user = getUserByToken(token);
    if (!user) return {};
    return user->waters;
}

bool HealthBackend::updateWater(const std::string& token,
                                std::size_t       index,
                                const std::string& newDatetime,
                                double             newAmountMl) {
    if (newAmountMl <= 0.0) return false;
    UserData* user = getUserByToken(token);
    if (!user) return false;
    if (index >= user->waters.size()) return false;

    user->waters[index].datetime = newDatetime;
    user->waters[index].amountMl = newAmountMl;
    saveToFile();
    return true;
}

bool HealthBackend::deleteWater(const std::string& token,
                                std::size_t       index) {
    UserData* user = getUserByToken(token);
    if (!user) return false;
    if (index >= user->waters.size()) return false;

    user->waters.erase(user->waters.begin() + static_cast<long>(index));
    saveToFile();
    return true;
}

// ----------------------
// Sleeps
// ----------------------

bool HealthBackend::addSleep(const std::string& token,
                             const std::string& datetime,
                             double             hours) {
    if (hours < 0.0) return false;
    UserData* user = getUserByToken(token);
    if (!user) return false;

    SleepRecord s;
    s.datetime = datetime;
    s.hours    = hours;
    user->sleeps.push_back(s);
    saveToFile();
    return true;
}

std::vector<SleepRecord> HealthBackend::getAllSleep(const std::string& token) const {
    const UserData* user = getUserByToken(token);
    if (!user) return {};
    return user->sleeps;
}

bool HealthBackend::updateSleep(const std::string& token,
                                std::size_t       index,
                                const std::string& newDatetime,
                                double             newHours) {
    if (newHours < 0.0) return false;
    UserData* user = getUserByToken(token);
    if (!user) return false;
    if (index >= user->sleeps.size()) return false;

    user->sleeps[index].datetime = newDatetime;
    user->sleeps[index].hours    = newHours;
    saveToFile();
    return true;
}

bool HealthBackend::deleteSleep(const std::string& token,
                                std::size_t       index) {
    UserData* user = getUserByToken(token);
    if (!user) return false;
    if (index >= user->sleeps.size()) return false;

    user->sleeps.erase(user->sleeps.begin() + static_cast<long>(index));
    saveToFile();
    return true;
}

// ----------------------
// Activities
// ----------------------

bool HealthBackend::addActivity(const std::string& token,
                                const std::string& datetime,
                                int                minutes,
                                const std::string& intensity) {
    if (minutes <= 0) return false;
    UserData* user = getUserByToken(token);
    if (!user) return false;

    ActivityRecord a;
    a.datetime  = datetime;
    a.minutes   = minutes;
    a.intensity = intensity;
    user->activities.push_back(a);
    saveToFile();
    return true;
}

std::vector<ActivityRecord> HealthBackend::getAllActivity(const std::string& token) const {
    const UserData* user = getUserByToken(token);
    if (!user) return {};
    return user->activities;
}

bool HealthBackend::updateActivity(const std::string& token,
                                   std::size_t       index,
                                   const std::string& newDatetime,
                                   int                newMinutes,
                                   const std::string& newIntensity) {
    if (newMinutes <= 0) return false;
    UserData* user = getUserByToken(token);
    if (!user) return false;
    if (index >= user->activities.size()) return false;

    user->activities[index].datetime  = newDatetime;
    user->activities[index].minutes   = newMinutes;
    user->activities[index].intensity = newIntensity;
    saveToFile();
    return true;
}

bool HealthBackend::deleteActivity(const std::string& token,
                                   std::size_t       index) {
    UserData* user = getUserByToken(token);
    if (!user) return false;
    if (index >= user->activities.size()) return false;

    user->activities.erase(user->activities.begin() + static_cast<long>(index));
    saveToFile();
    return true;
}

// ----------------------
// Custom Categories
// ----------------------

std::vector<std::string> HealthBackend::getOtherCategories(const std::string& token) const {
    const UserData* user = getUserByToken(token);
    if (!user) return {};

    std::vector<std::string> cats;
    for (const auto& [name, _vec] : user->categories) {
        cats.push_back(name);
    }
    return cats;
}

bool HealthBackend::createCategory(const std::string& token,
                                   const std::string& name)
{
    if (name.empty()) return false;
    UserData* user = getUserByToken(token);
    if (!user) return false;

    if (user->categories.find(name) != user->categories.end())
        return false; // 已存在

    user->categories[name] = {};  // 建立空 category
    saveToFile();
    return true;
}

// ⚠️ 不再自動建立 category
bool HealthBackend::addOtherRecord(const std::string& token,
                                   const std::string& categoryName,
                                   const std::string& datetime,
                                   double             value,
                                   const std::string& note) 
{
    UserData* user = getUserByToken(token);
    if (!user) return false;

    auto it = user->categories.find(categoryName);
    if (it == user->categories.end())
        return false;              // ❌ category 不存在 → 回傳 false

    CategoryItem item;
    item.datetime = datetime;
    item.note     = note;
    item.value    = value;

    it->second.push_back(item);
    saveToFile();
    return true;
}

std::vector<CategoryItem> HealthBackend::getOtherRecords(const std::string& token,
                                                         const std::string& categoryName) const {
    const UserData* user = getUserByToken(token);
    if (!user) return {};
    auto it = user->categories.find(categoryName);
    if (it == user->categories.end()) return {};
    return it->second;
}

bool HealthBackend::updateOtherRecord(const std::string& token,
                                      const std::string& categoryName,
                                      std::size_t       index,
                                      const std::string& newDatetime,
                                      double             newValue,
                                      const std::string& newNote) {
    UserData* user = getUserByToken(token);
    if (!user) return false;
    auto it = user->categories.find(categoryName);
    if (it == user->categories.end()) return false;
    auto& vec = it->second;
    if (index >= vec.size()) return false;

    vec[index].datetime = newDatetime;
    vec[index].note     = newNote;
    vec[index].value    = newValue;
    saveToFile();
    return true;
}

bool HealthBackend::deleteOtherRecord(const std::string& token,
                                      const std::string& categoryName,
                                      std::size_t       index) {
    UserData* user = getUserByToken(token);
    if (!user) return false;
    auto it = user->categories.find(categoryName);
    if (it == user->categories.end()) return false;

    auto& vec = it->second;
    if (index >= vec.size()) return false;

    vec.erase(vec.begin() + static_cast<long>(index));
    saveToFile();
    return true;
}