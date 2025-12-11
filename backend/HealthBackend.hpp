#ifndef HEALTH_BACKEND_HPP
#define HEALTH_BACKEND_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <map>

#include "../external/json.hpp"

// 使用 nlohmann::json
using json = nlohmann::ordered_json;

// ----------------------
// 資料結構定義
// ----------------------

struct UserProfile {
    std::string id;       // 可以用 name 當 id
    std::string name;
    int         age = 0;
    double      weightKg = 0.0;
    double      heightM  = 0.0;
    std::string gender;   // "male" | "female" | "other"
};

struct WaterRecord {
    std::string datetime;  // spec: ISO8601 datetime string
    double      amountMl = 0.0;
};

struct SleepRecord {
    std::string datetime;
    double      hours = 0.0;
};

struct ActivityRecord {
    std::string datetime;
    int         minutes = 0;
    std::string intensity;  // "low" | "moderate" | "high"
};

struct CategoryItem {
    std::string datetime;
    std::string note;
    double      value = 0.0;  // 後端內部使用，API 不會暴露
};

class HealthBackend {
public:
    HealthBackend();
    ~HealthBackend();

    // ---------- User / Auth ----------
    // 註冊：成功回 true，失敗 (重複名稱或無效) 回 false
    bool registerUser(const std::string& name,
                      int                age,
                      double             weightKg,
                      double             heightM,
                      const std::string& password,
                      const std::string& gender);

    // 登入：成功回 token，失敗回 "INVALID"
    std::string login(const std::string& name,
                      const std::string& password);

    // 檢查 token 對應的 user 是否存在
    bool hasUserForToken(const std::string& token) const;

    // 取得 user profile（給 /user/profile 用）
    bool getUserProfile(const std::string& token,
                        UserProfile&       outProfile) const;

    // 取得 BMI（kg / m^2），無效 token 或資料錯誤回 <=0
    double getBMI(const std::string& token) const;

    // ---------- Waters ----------
    bool addWater(const std::string& token,
                  const std::string& datetime,
                  double             amountMl);

    std::vector<WaterRecord> getAllWater(const std::string& token) const;

    bool updateWater(const std::string& token,
                     std::size_t       index,
                     const std::string& newDatetime,
                     double             newAmountMl);

    bool deleteWater(const std::string& token,
                     std::size_t       index);

    // ---------- Sleeps ----------
    bool addSleep(const std::string& token,
                  const std::string& datetime,
                  double             hours);

    std::vector<SleepRecord> getAllSleep(const std::string& token) const;

    bool updateSleep(const std::string& token,
                     std::size_t       index,
                     const std::string& newDatetime,
                     double             newHours);

    bool deleteSleep(const std::string& token,
                     std::size_t       index);

    // ---------- Activities ----------
    bool addActivity(const std::string& token,
                     const std::string& datetime,
                     int                minutes,
                     const std::string& intensity);

    std::vector<ActivityRecord> getAllActivity(const std::string& token) const;

    bool updateActivity(const std::string& token,
                        std::size_t       index,
                        const std::string& newDatetime,
                        int                newMinutes,
                        const std::string& newIntensity);

    bool deleteActivity(const std::string& token,
                        std::size_t       index);

    // ---------- Custom Categories ----------
    // 回傳所有 categoryName（每個 user 獨立）
    std::vector<std::string> getOtherCategories(const std::string& token) const;

    bool addOtherRecord(const std::string& token,
                        const std::string& categoryName,
                        const std::string& datetime,
                        double             value,
                        const std::string& note);

    std::vector<CategoryItem> getOtherRecords(const std::string& token,
                                              const std::string& categoryName) const;

    bool updateOtherRecord(const std::string& token,
                           const std::string& categoryName,
                           std::size_t       index,
                           const std::string& newDatetime,
                           double             newValue,
                           const std::string& newNote);

    bool deleteOtherRecord(const std::string& token,
                           const std::string& categoryName,
                           std::size_t       index);
    
    bool createCategory(const std::string& token, const std::string& name);
                    
private:
    // 每個 user 的完整資料
    struct UserData {
        UserProfile profile;
        std::string password;

        std::vector<WaterRecord>     waters;
        std::vector<SleepRecord>     sleeps;
        std::vector<ActivityRecord>  activities;
        std::map<std::string, std::vector<CategoryItem>> categories; // 用 map 讓順序穩定
    };

    // 以 name 當 key
    std::unordered_map<std::string, UserData> usersByName;
    // token -> name
    std::unordered_map<std::string, std::string> tokenToName;

    std::string storagePath;

    // 載入 / 儲存
    void loadFromFile();
    void saveToFile() const;

    // 內部：透過 token 找 user
    UserData*       getUserByToken(const std::string& token);
    const UserData* getUserByToken(const std::string& token) const;

    // 產生隨機 token（假裝是 JWT）
    std::string generateToken() const;
};

#endif // HEALTH_BACKEND_HPP