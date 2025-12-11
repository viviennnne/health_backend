#pragma once

#include <string>
#include <vector>
#include <map>

// ----------------------
// 基本資料結構
// ----------------------

struct UserProfile {
    std::string id;
    std::string name;
    int         age = 0;
    double      weightKg = 0.0;
    double      heightM  = 0.0;
    std::string gender;
};

struct WaterRecord {
    std::string datetime;
    double      amountMl = 0.0;
};

struct SleepRecord {
    std::string datetime;
    double      hours = 0.0;
};

struct ActivityRecord {
    std::string datetime;
    int         minutes = 0;
    std::string intensity;
};

struct CategoryItem {
    std::string datetime;
    std::string note;
    double      value = 0.0;
};

class HealthBackend {
public:
    struct UserData {
        UserProfile profile;
        std::string password;

        std::vector<WaterRecord>    waters;
        std::vector<SleepRecord>    sleeps;
        std::vector<ActivityRecord> activities;

        // categoryName → items
        std::map<std::string, std::vector<CategoryItem>> categories;
    };

    HealthBackend();
    ~HealthBackend();

    // -------- User / Auth --------
    bool        registerUser(const std::string& name,
                             int                age,
                             double             weightKg,
                             double             heightM,
                             const std::string& password,
                             const std::string& gender);
    std::string login(const std::string& name,
                      const std::string& password);

    bool   getUserProfile(const std::string& token,
                          UserProfile&       outProfile) const;
    double getBMI(const std::string& token) const;

    bool   hasUserForToken(const std::string& token) const;

    // -------- Water --------
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

    // -------- Sleep --------
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

    // -------- Activity --------
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

    // -------- Custom Categories --------
    std::vector<std::string> getOtherCategories(const std::string& token) const;

    bool createCategory(const std::string& token,
                        const std::string& name);

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

    bool deleteCategory(const std::string& token,
                        const std::string& categoryName);

private:
    std::map<std::string, UserData>    usersByName;
    std::map<std::string, std::string> tokenToName;

    std::string storagePath;

    // 檔案 / 路徑相關
    void initStoragePath();             // 設定 storagePath
    void ensureStorageDirExists() const; // 確保資料夾存在

    // JSON I/O
    void loadFromFile();
    void saveToFile() const;

    // Token / 使用者
    std::string generateToken() const;
    UserData*       getUserByToken(const std::string& token);
    const UserData* getUserByToken(const std::string& token) const;
};