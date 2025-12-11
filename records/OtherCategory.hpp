#ifndef OTHER_CATEGORY_HPP
#define OTHER_CATEGORY_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include "../external/json.hpp"   // 使用 nlohmann::json

struct OtherRecord {
    std::string date;   // "YYYY-MM-DD"
    double      value;  // 數值
    std::string note;   // 備註
};

class OtherCategoryManager {
private:
    // userName -> categoryName -> records
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::vector<OtherRecord>>
    > data;

public:
    bool addRecord(const std::string& userName,
                   const std::string& categoryName,
                   const std::string& date,
                   double value,
                   const std::string& note);

    bool updateRecord(const std::string& userName,
                      const std::string& categoryName,
                      std::size_t index,
                      const std::string& newDate,
                      double newValue,
                      const std::string& newNote);

    bool deleteRecord(const std::string& userName,
                      const std::string& categoryName,
                      std::size_t index);

    std::vector<std::string> getCategories(const std::string& userName) const;

    std::vector<OtherRecord> getRecords(const std::string& userName,
                                        const std::string& categoryName) const;

    // JSON 匯出 / 匯入
    nlohmann::ordered_json toJson() const;
    void fromJson(const nlohmann::ordered_json& j);
};

#endif