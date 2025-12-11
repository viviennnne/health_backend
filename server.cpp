#include <iostream>
#include <string>
#include <vector>

#include "httplib.h"
#include "backend/HealthBackend.hpp"
#include "external/json.hpp"

// 從 Authorization header 取出 Bearer token
// 規格：Authorization: Bearer <jwt>
std::string getTokenFromAuthHeader(const httplib::Request &req) {
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end()) {
        return "";
    }
    const std::string &auth = it->second;
    const std::string prefix = "Bearer ";
    if (auth.size() >= prefix.size() &&
        auth.compare(0, prefix.size(), prefix) == 0) {
        return auth.substr(prefix.size());
    }
    return "";
}

// 將 0-based index 轉成對外用的 id 字串，例如 index=0 -> "item-1"
std::string makeCategoryItemId(std::size_t index) {
    return "item-" + std::to_string(index + 1);
}

// 將前端傳來的 id 轉回 0-based index
// 支援兩種格式：
//   1. "item-3" -> index = 2
//   2. "5"      -> index = 5  (純數字也允許，方便你自己測試)
bool parseCategoryItemId(const std::string &idStr, std::size_t &index) {
    try {
        if (idStr.rfind("item-", 0) == 0) {
            // 開頭是 "item-"
            std::string numStr = idStr.substr(5); // 拿掉 "item-"
            std::size_t n = static_cast<std::size_t>(std::stoul(numStr));
            if (n == 0) return false;            // 不接受 item-0
            index = n - 1;                       // 轉成 0-based
            return true;
        } else {
            // 純數字也接受
            index = static_cast<std::size_t>(std::stoul(idStr));
            return true;
        }
    } catch (...) {
        return false;
    }
}

int main() {
    HealthBackend backend;
    httplib::Server svr;

    // CORS: respond to preflight and add headers to every response
    svr.Options(R"(.*)", [](const httplib::Request &req, httplib::Response &res) {
        // Allow requests from any origin (adjust if you want to restrict)
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.set_header("Access-Control-Max-Age", "3600");
        res.status = 204; // No Content for preflight
    });

    // Inject CORS headers to all responses via a post-routing hook
    svr.set_post_routing_handler([&](const httplib::Request & /*req*/, httplib::Response &res) {
        // Only add header if it's not already present
        if (res.get_header_value("Access-Control-Allow-Origin").empty()) {
            res.set_header("Access-Control-Allow-Origin", "*");
        }
        if (res.get_header_value("Access-Control-Allow-Headers").empty()) {
            res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        }
        if (res.get_header_value("Access-Control-Allow-Methods").empty()) {
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS");
        }
    });

    // =======================
    //      Health Check
    // =======================
    svr.Get("/health", [](const httplib::Request &, httplib::Response &res) {
        json j;
        j["status"]  = "ok";
        j["message"] = "health_backend server running";
        res.status = 200;
        res.set_content(j.dump(), "application/json");
    });

    // =======================
    //   Authentication / User
    // =======================

    // POST /register
    // Body: { "name","password","age","weightKg","heightM","gender" }
    // 回傳: 201 { "token": "..." }
    svr.Post("/register", [&backend](const httplib::Request &req, httplib::Response &res) {
        try {
            json j = json::parse(req.body);

            // 檢查必要欄位
            if (!j.contains("name") ||
                !j.contains("password") ||
                !j.contains("age") ||
                !j.contains("weightKg") ||
                !j.contains("heightM") ||
                !j.contains("gender")) {
                json err;
                err["errorMessage"] = "Missing or invalid fields";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::string name     = j["name"].get<std::string>();
            std::string password = j["password"].get<std::string>();
            int         age      = j["age"].get<int>();
            double      weightKg = j["weightKg"].get<double>();
            double      heightM  = j["heightM"].get<double>();
            std::string gender   = j["gender"].get<std::string>();

            bool ok = backend.registerUser(name, age, weightKg, heightM, password, gender);
            if (!ok) {
                json err;
                err["errorMessage"] = "User already exists";
                res.status = 409;
                res.set_content(err.dump(), "application/json");
                return;
            }

            // 註冊成功後，直接幫他登入一次取得 token
            std::string token = backend.login(name, password);
            if (token == "INVALID") {
                json err;
                err["errorMessage"] = "Internal error when generating token";
                res.status = 500;
                res.set_content(err.dump(), "application/json");
                return;
            }

            json out;
            out["token"] = token;
            res.status = 201;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception &e) {
            json err;
            err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    // POST /login
    // Body: { "name","password" }
    // 回傳: 200 { "token":"..." }
    svr.Post("/login", [&backend](const httplib::Request &req, httplib::Response &res) {
        try {
            json j = json::parse(req.body);

            if (!j.contains("name") || !j.contains("password")) {
                json err;
                err["errorMessage"] = "Missing name or password";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::string name     = j["name"].get<std::string>();
            std::string password = j["password"].get<std::string>();

            std::string token = backend.login(name, password);
            if (token == "INVALID") {
                json err;
                err["errorMessage"] = "Invalid name or password";
                res.status = 401;
                res.set_content(err.dump(), "application/json");
                return;
            }

            json out;
            out["token"] = token;
            res.status = 200;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception &e) {
            json err;
            err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    // GET /user/profile
    svr.Get("/user/profile", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        UserProfile profile;
        if (!backend.getUserProfile(token, profile)) {
            json err;
            err["errorMessage"] = "Profile not found";
            res.status = 404;
            res.set_content(err.dump(), "application/json");
            return;
        }

        json out;
        out["id"]       = profile.id;
        out["name"]     = profile.name;
        out["gender"]   = profile.gender;
        out["weightKg"] = profile.weightKg;
        out["heightM"]  = profile.heightM;
        out["age"]      = profile.age;

        res.status = 200;
        res.set_content(out.dump(), "application/json");
    });

    // GET /user/bmi
    svr.Get("/user/bmi", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        double bmi = backend.getBMI(token);
        if (bmi <= 0.0) {  // 簡單判斷，當成找不到 profile
            json err;
            err["errorMessage"] = "Profile not found";
            res.status = 404;
            res.set_content(err.dump(), "application/json");
            return;
        }

        json out;
        out["bmi"] = bmi;
        res.status = 200;
        res.set_content(out.dump(), "application/json");
    });

    // =======================
    //        Waters
    // =======================

    // POST /waters
    // Body: { "datetime":"...","amountMl":250 }
    svr.Post("/waters", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        try {
            json j = json::parse(req.body);
            if (!j.contains("datetime") || !j.contains("amountMl")) {
                json err;
                err["errorMessage"] = "Missing datetime or amountMl";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::string datetime = j["datetime"].get<std::string>();
            double      amount   = j["amountMl"].get<double>();

            bool ok = backend.addWater(token, datetime, amount);
            if (!ok) {
                json err;
                err["errorMessage"] = "Failed to add water record";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            auto records = backend.getAllWater(token);
            if (records.empty()) {
                json err;
                err["errorMessage"] = "Internal error: no records after add";
                res.status = 500;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::size_t idx = records.size() - 1;
            const auto &r   = records[idx];

            json out;
            out["id"]       = std::to_string(idx);  // index 當作 id
            out["datetime"] = r.datetime;
            out["amountMl"] = r.amountMl;
            res.status = 201;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception &e) {
            json err;
            err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    // GET /waters
    svr.Get("/waters", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        auto records = backend.getAllWater(token);

        json arr = json::array();
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto &r = records[i];
            json jr;
            jr["id"]       = std::to_string(i);
            jr["datetime"] = r.datetime;
            jr["amountMl"] = r.amountMl;
            arr.push_back(jr);
        }

        res.status = 200;
        res.set_content(arr.dump(), "application/json");
    });

    // PATCH /waters/{id}
    svr.Patch(R"(/waters/(\d+))", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string idStr = req.matches[1];
        std::size_t index = 0;
        try {
            index = static_cast<std::size_t>(std::stoul(idStr));
        } catch (...) {
            json err;
            err["errorMessage"] = "Invalid water id";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        try {
            json j = json::parse(req.body);

            auto records = backend.getAllWater(token);
            if (index >= records.size()) {
                json err;
                err["errorMessage"] = "Record not found";
                res.status = 404;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::string newDatetime = records[index].datetime;
            double      newAmount   = records[index].amountMl;

            if (j.contains("datetime")) {
                newDatetime = j["datetime"].get<std::string>();
            }
            if (j.contains("amountMl")) {
                newAmount = j["amountMl"].get<double>();
            }

            bool ok = backend.updateWater(token, index, newDatetime, newAmount);
            if (!ok) {
                json err;
                err["errorMessage"] = "Failed to update water record";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            json out;
            out["id"]       = idStr;
            out["datetime"] = newDatetime;
            out["amountMl"] = newAmount;
            res.status = 200;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception &e) {
            json err;
            err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    // DELETE /waters/{id}
    svr.Delete(R"(/waters/(\d+))", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string idStr = req.matches[1];
        std::size_t index = 0;
        try {
            index = static_cast<std::size_t>(std::stoul(idStr));
        } catch (...) {
            json err;
            err["errorMessage"] = "Invalid water id";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        bool ok = backend.deleteWater(token, index);
        if (!ok) {
            json err;
            err["errorMessage"] = "Record not found";
            res.status = 404;
            res.set_content(err.dump(), "application/json");
            return;
        }

        res.status = 204;  // No Content
        res.set_content("", "application/json");
    });

    // =======================
    //         Sleeps
    // =======================

    // POST /sleeps
    // Body: { "datetime":"...","hours":7.5 }
    svr.Post("/sleeps", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        try {
            json j = json::parse(req.body);
            if (!j.contains("datetime") || !j.contains("hours")) {
                json err;
                err["errorMessage"] = "Missing datetime or hours";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::string datetime = j["datetime"].get<std::string>();
            double      hours    = j["hours"].get<double>();

            bool ok = backend.addSleep(token, datetime, hours);
            if (!ok) {
                json err;
                err["errorMessage"] = "Failed to add sleep record";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            auto records = backend.getAllSleep(token);
            if (records.empty()) {
                json err;
                err["errorMessage"] = "Internal error: no sleep records after add";
                res.status = 500;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::size_t idx = records.size() - 1;
            const auto &r   = records[idx];

            json out;
            out["id"]       = std::to_string(idx);
            out["datetime"] = r.datetime;
            out["hours"]    = r.hours;
            res.status = 201;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception &e) {
            json err;
            err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    // GET /sleeps
    svr.Get("/sleeps", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        auto records = backend.getAllSleep(token);

        json arr = json::array();
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto &r = records[i];
            json jr;
            jr["id"]       = std::to_string(i);
            jr["datetime"] = r.datetime;
            jr["hours"]    = r.hours;
            arr.push_back(jr);
        }

        res.status = 200;
        res.set_content(arr.dump(), "application/json");
    });

    // PATCH /sleeps/{id}
    svr.Patch(R"(/sleeps/(\d+))", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string idStr = req.matches[1];
        std::size_t index = 0;
        try {
            index = static_cast<std::size_t>(std::stoul(idStr));
        } catch (...) {
            json err;
            err["errorMessage"] = "Invalid sleep id";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        try {
            json j = json::parse(req.body);

            auto records = backend.getAllSleep(token);
            if (index >= records.size()) {
                json err;
                err["errorMessage"] = "Record not found";
                res.status = 404;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::string newDatetime = records[index].datetime;
            double      newHours    = records[index].hours;

            if (j.contains("datetime")) {
                newDatetime = j["datetime"].get<std::string>();
            }
            if (j.contains("hours")) {
                newHours = j["hours"].get<double>();
            }

            bool ok = backend.updateSleep(token, index, newDatetime, newHours);
            if (!ok) {
                json err;
                err["errorMessage"] = "Failed to update sleep record";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            json out;
            out["id"]       = idStr;
            out["datetime"] = newDatetime;
            out["hours"]    = newHours;
            res.status = 200;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception &e) {
            json err;
            err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    // DELETE /sleeps/{id}
    svr.Delete(R"(/sleeps/(\d+))", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string idStr = req.matches[1];
        std::size_t index = 0;
        try {
            index = static_cast<std::size_t>(std::stoul(idStr));
        } catch (...) {
            json err;
            err["errorMessage"] = "Invalid sleep id";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        bool ok = backend.deleteSleep(token, index);
        if (!ok) {
            json err;
            err["errorMessage"] = "Record not found";
            res.status = 404;
            res.set_content(err.dump(), "application/json");
            return;
        }

        res.status = 204;
        res.set_content("", "application/json");
    });

    // =======================
    //       Activities
    // =======================

    // POST /activities
    // Body: { "datetime":"...","minutes":30,"intensity":"moderate" }
    svr.Post("/activities", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        try {
            json j = json::parse(req.body);
            if (!j.contains("datetime") ||
                !j.contains("minutes") ||
                !j.contains("intensity")) {
                json err;
                err["errorMessage"] = "Missing fields";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::string datetime  = j["datetime"].get<std::string>();
            int         minutes   = j["minutes"].get<int>();
            std::string intensity = j["intensity"].get<std::string>();

            bool ok = backend.addActivity(token, datetime, minutes, intensity);
            if (!ok) {
                json err;
                err["errorMessage"] = "Failed to add activity record";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            auto records = backend.getAllActivity(token);
            if (records.empty()) {
                json err;
                err["errorMessage"] = "Internal error: no activity records after add";
                res.status = 500;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::size_t idx = records.size() - 1;
            const auto &a   = records[idx];

            json out;
            out["id"]        = std::to_string(idx);
            out["datetime"]  = a.datetime;
            out["minutes"]   = a.minutes;
            out["intensity"] = a.intensity;
            res.status = 201;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception &e) {
            json err;
            err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    // GET /activities
    svr.Get("/activities", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        auto records = backend.getAllActivity(token);

        json arr = json::array();
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto &a = records[i];
            json ja;
            ja["id"]        = std::to_string(i);
            ja["datetime"]  = a.datetime;
            ja["minutes"]   = a.minutes;
            ja["intensity"] = a.intensity;
            arr.push_back(ja);
        }

        res.status = 200;
        res.set_content(arr.dump(), "application/json");
    });

    // PATCH /activities/{id}
    svr.Patch(R"(/activities/(\d+))", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string idStr = req.matches[1];
        std::size_t index = 0;
        try {
            index = static_cast<std::size_t>(std::stoul(idStr));
        } catch (...) {
            json err;
            err["errorMessage"] = "Invalid activity id";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        try {
            json j = json::parse(req.body);
            auto records = backend.getAllActivity(token);
            if (index >= records.size()) {
                json err;
                err["errorMessage"] = "Record not found";
                res.status = 404;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::string newDatetime  = records[index].datetime;
            int         newMinutes   = records[index].minutes;
            std::string newIntensity = records[index].intensity;

            if (j.contains("datetime")) {
                newDatetime = j["datetime"].get<std::string>();
            }
            if (j.contains("minutes")) {
                newMinutes = j["minutes"].get<int>();
            }
            if (j.contains("intensity")) {
                newIntensity = j["intensity"].get<std::string>();
            }

            bool ok = backend.updateActivity(token, index, newDatetime, newMinutes, newIntensity);
            if (!ok) {
                json err;
                err["errorMessage"] = "Failed to update activity record";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            json out;
            out["id"]        = idStr;
            out["datetime"]  = newDatetime;
            out["minutes"]   = newMinutes;
            out["intensity"] = newIntensity;
            res.status = 200;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception &e) {
            json err;
            err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    // DELETE /activities/{id}
    svr.Delete(R"(/activities/(\d+))", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string idStr = req.matches[1];
        std::size_t index = 0;
        try {
            index = static_cast<std::size_t>(std::stoul(idStr));
        } catch (...) {
            json err;
            err["errorMessage"] = "Invalid activity id";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        bool ok = backend.deleteActivity(token, index);
        if (!ok) {
            json err;
            err["errorMessage"] = "Record not found";
            res.status = 404;
            res.set_content(err.dump(), "application/json");
            return;
        }

        res.status = 204;
        res.set_content("", "application/json");
    });

    // =======================
    //   Custom Categories
    // =======================

    // GET /category/list
    svr.Get("/category/list", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        auto cats = backend.getOtherCategories(token);

        json arr = json::array();
        for (const auto &name : cats) {
            json jc;
            jc["id"]           = name;  // 名稱當 id
            jc["categoryName"] = name;
            arr.push_back(jc);
        }

        res.status = 200;
        res.set_content(arr.dump(), "application/json");
    });

    // POST /category/create
    svr.Post("/category/create", [&backend](const httplib::Request &req, httplib::Response &res) {
    std::string token = getTokenFromAuthHeader(req);
    if (token.empty()) {
        json err; err["errorMessage"] = "Missing or invalid Authorization token";
        res.status = 401;
        res.set_content(err.dump(), "application/json");
        return;
    }

    try {
        json j = json::parse(req.body);

        if (!j.contains("categoryName")) {
            json err; err["errorMessage"] = "Missing categoryName";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string name = j["categoryName"].get<std::string>();

        // ⚠️ 這裡新增 category（空 vector）
        if (!backend.createCategory(token, name)) {
            json err; err["errorMessage"] = "Category already exists or invalid name";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        json out;
        out["id"] = name;
        out["categoryName"] = name;

        res.status = 201;
        res.set_content(out.dump(), "application/json");
    }
    catch (...) {
        json err; err["errorMessage"] = "Invalid JSON";
        res.status = 400;
        res.set_content(err.dump(), "application/json");
    }
});

    // GET /category/{categoryId}/list
    svr.Get(R"(/category/([^/]+)/list)", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string categoryId = req.matches[1];

        auto records = backend.getOtherRecords(token, categoryId);
        if (records.empty()) {
            json err;
            err["errorMessage"] = "Category not found or no items";
            res.status = 404;
            res.set_content(err.dump(), "application/json");
            return;
        }

        json arr = json::array();
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto &r = records[i];
            json jr;
            jr["id"]       = makeCategoryItemId(i);
            jr["datetime"] = r.datetime;
            jr["note"]     = r.note;
            arr.push_back(jr);
        }

        res.status = 200;
        res.set_content(arr.dump(), "application/json");
    });

    // POST /category/{categoryId}/add
    svr.Post(R"(/category/([^/]+)/add)", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string categoryId = req.matches[1];

        try {
            json j = json::parse(req.body);
            if (!j.contains("datetime") || !j.contains("note")) {
                json err;
                err["errorMessage"] = "Missing datetime or note";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::string datetime = j["datetime"].get<std::string>();
            std::string note     = j["note"].get<std::string>();

            // 後端需要 value，我們用 0.0 佔位
            bool ok = backend.addOtherRecord(token, categoryId, datetime, 0.0, note);
            if (!ok) {
                json err;
                err["errorMessage"] = "Failed to add category item";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            auto records = backend.getOtherRecords(token, categoryId);
            std::size_t idx = records.size() - 1;
            const auto &r   = records[idx];

            json out;
            out["id"]         = std::to_string(idx);
            out["categoryId"] = categoryId;
            out["datetime"]   = r.datetime;
            out["note"]       = r.note;
            res.status = 201;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception &e) {
            json err;
            err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    // PATCH /category/{categoryId}/{itemId}
    svr.Patch(R"(/category/([^/]+)/(\d+))", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string categoryId = req.matches[1];
        std::string itemIdStr  = req.matches[2];

        std::size_t index = 0;
        try {
            index = static_cast<std::size_t>(std::stoul(itemIdStr));
        } catch (...) {
            json err;
            err["errorMessage"] = "Invalid item id";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        try {
            json j = json::parse(req.body);
            auto records = backend.getOtherRecords(token, categoryId);
            if (index >= records.size()) {
                json err;
                err["errorMessage"] = "Category or item not found";
                res.status = 404;
                res.set_content(err.dump(), "application/json");
                return;
            }

            std::string newDatetime = records[index].datetime;
            std::string newNote     = records[index].note;
            double      value       = records[index].value;

            if (j.contains("datetime")) {
                newDatetime = j["datetime"].get<std::string>();
            }
            if (j.contains("note")) {
                newNote = j["note"].get<std::string>();
            }

            bool ok = backend.updateOtherRecord(token, categoryId, index,
                                                newDatetime, value, newNote);
            if (!ok) {
                json err;
                err["errorMessage"] = "Failed to update category item";
                res.status = 400;
                res.set_content(err.dump(), "application/json");
                return;
            }

            json out;
            out["id"]         = itemIdStr;
            out["categoryId"] = categoryId;
            out["datetime"]   = newDatetime;
            out["note"]       = newNote;
            res.status = 200;
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception &e) {
            json err;
            err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    // DELETE /category/{categoryId}/{itemId}
    svr.Delete(R"(/category/([^/]+)/(\d+))", [&backend](const httplib::Request &req, httplib::Response &res) {
        std::string token = getTokenFromAuthHeader(req);
        if (token.empty()) {
            json err;
            err["errorMessage"] = "Missing or invalid Authorization token";
            res.status = 401;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string categoryId = req.matches[1];
        std::string itemIdStr  = req.matches[2];

        std::size_t index = 0;
        try {
            index = static_cast<std::size_t>(std::stoul(itemIdStr));
        } catch (...) {
            json err;
            err["errorMessage"] = "Invalid item id";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        bool ok = backend.deleteOtherRecord(token, categoryId, index);
        if (!ok) {
            json err;
            err["errorMessage"] = "Category or item not found";
            res.status = 404;
            res.set_content(err.dump(), "application/json");
            return;
        }

        res.status = 204;
        res.set_content("", "application/json");
    });

    std::cout << "Server started at http://0.0.0.0:8080\n";
    svr.listen("0.0.0.0", 8080);

    return 0;
}