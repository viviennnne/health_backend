#ifndef USER_BACKEND_HPP
#define USER_BACKEND_HPP

#include <string>
#include <unordered_map>
#include "../external/json.hpp"
#include "User.hpp"

using json = nlohmann::ordered_json;

class UserBackend {
private:
    // key = user name
    std::unordered_map<std::string, User> users;

    // token -> user name
    std::unordered_map<std::string, std::string> tokenToName;

    std::string generateToken() const;

public:
    // 註冊：新增一個 user（包含 gender）
    bool registerUser(const std::string& name,
                      int age,
                      double weightKg,
                      double heightM,
                      const std::string& password,
                      const std::string& gender);

    // 登入：回傳 token（若已有 token 會沿用，沒有就重新產生）
    std::string login(const std::string& name,
                      const std::string& password);

    // 依「名字」更新使用者基本資料（含 gender）
    bool updateUser(const std::string& name,
                    int newAge,
                    double newWeightKg,
                    double newHeightM,
                    const std::string& newPassword,
                    const std::string& newGender);

    // 刪除使用者（會一併清 token map）
    bool deleteUser(const std::string& name);

    // 用 token 算 BMI
    double getUserBMI(const std::string& token) const;

    // 用 token 找出 userName（找不到回傳空字串）
    std::string getUserNameByToken(const std::string& token) const;

    // 用 name 找 User 物件（找不到回傳 nullptr）
    const User* findUserByName(const std::string& name) const;

    // 存檔 / 讀檔用 JSON
    json toJson() const;
    void fromJson(const json& j);
};

#endif // USER_BACKEND_HPP