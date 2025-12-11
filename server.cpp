// server.cpp
// ===== CHANGED: 加上 CORS、修好 Category 建立/新增/刪除流程 =====

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "backend/HealthBackend.hpp"
#include "external/json.hpp"
#include "helpers/Logger.hpp"
#include "httplib.h"

using json = nlohmann::ordered_json;

using json = nlohmann::ordered_json;

// 從 Authorization header 取出 Bearer token
// 規格：Authorization: Bearer <jwt>
std::string getTokenFromAuthHeader(const httplib::Request& req) {
  auto it = req.headers.find("Authorization");
  if (it == req.headers.end()) {
    return "";
  }
  const std::string& auth = it->second;
  const std::string prefix = "Bearer ";
  if (auth.size() >= prefix.size() && auth.compare(0, prefix.size(), prefix) == 0) {
    return auth.substr(prefix.size());
  }
  return "";
}

int main() {
  HealthBackend backend;
  httplib::Server svr;

  // Initialize logger -------------------------------
  const char* logFileEnv = std::getenv("LOG_FILE");
  std::string logFilePath = logFileEnv ? logFileEnv : "logs/server.log";
  const char* logLevelEnv = std::getenv("LOG_LEVEL");
  util::LogLevel level = util::LogLevel::Info;
  if (logLevelEnv) {
    std::string s = logLevelEnv;
    if (s == "DEBUG")
      level = util::LogLevel::Debug;
    else if (s == "WARN")
      level = util::LogLevel::Warning;
    else if (s == "ERROR")
      level = util::LogLevel::Error;
    else
      level = util::LogLevel::Info;
  }
  util::Logger::init(logFilePath, level);
  // --------------------------------------------------

  // ===== NEW: CORS 設定（前端在別的 Port/Domain 時也能用） =====
  svr.Options(R"(.*)", [](const httplib::Request& req, httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.set_header("Access-Control-Max-Age", "3600");
    res.status = 204;  // No Content
  });

  // Log exceptions
  svr.set_exception_handler([](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
    (void)res;  // we don't modify response here
    std::string origin = req.has_header("Origin") ? req.get_header_value("Origin") : "-";
    try {
      if (ep) std::rethrow_exception(ep);
    } catch (const std::exception& e) {
      util::Logger::error(std::string("Unhandled exception handling request: ") + req.method + " " + req.path +
                          " Origin:" + origin + " error: " + e.what());
    } catch (...) {
      util::Logger::error(std::string("Unhandled exception handling request: ") + req.method + " " + req.path +
                          " Origin:" + origin + " error: unknown");
    }
  });

  // We'll track request start times so we can log durations.
  static std::mutex _req_mtx;
  static std::unordered_map<const httplib::Request*, std::chrono::steady_clock::time_point> _req_start;

  // Pre-routing: record start time
  svr.set_pre_routing_handler(
      [&](const httplib::Request& req, httplib::Response& /*res*/) -> httplib::Server::HandlerResponse {
        std::lock_guard<std::mutex> lk(_req_mtx);
        _req_start[&req] = std::chrono::steady_clock::now();
        auto origin = req.has_header("Origin") ? req.get_header_value("Origin") : "-";
        util::Logger::info(req.method + std::string(" ") + req.path + " Origin:" + origin);
        return httplib::Server::HandlerResponse::Unhandled;
      });

  // Inject CORS headers and log request info via a post-routing hook
  svr.set_post_routing_handler([&](const httplib::Request& req, httplib::Response& res) {
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
    // Log request duration
    std::chrono::steady_clock::time_point start;
    {
      std::lock_guard<std::mutex> lk(_req_mtx);
      auto it = _req_start.find(&req);
      if (it != _req_start.end()) {
        start = it->second;
        _req_start.erase(it);
      }
    }
    if (start.time_since_epoch().count() > 0) {
      auto dur =
          std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
      util::Logger::info(req.method + std::string(" ") + req.path + " -> " + std::to_string(res.status) + " (" +
                         std::to_string(dur) + " ms)");
    }
  });

  // Set httplib server-level logging to route through our Logger
  svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
    (void)res;
    auto origin = req.has_header("Origin") ? req.get_header_value("Origin") : "-";
    util::Logger::debug(std::string("httplib log: ") + req.method + " " + req.path + " Origin:" + origin);
  });
  svr.set_error_logger([](const httplib::Error& err, const httplib::Request* req) {
    std::string path = req ? req->path : "-";
    util::Logger::warn(std::string("httplib error: ") + std::to_string(static_cast<int>(err)) + " path:" + path);
  });

  // ======================
  //      Health Check
  // =======================
  svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
    json j;
    j["status"] = "ok";
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
  svr.Post("/register", [&backend](const httplib::Request& req, httplib::Response& res) {
    try {
      json j = json::parse(req.body);

      if (!j.contains("name") || !j.contains("password") || !j.contains("age") || !j.contains("weightKg") ||
          !j.contains("heightM") || !j.contains("gender")) {
        json err;
        err["errorMessage"] = "Missing or invalid fields";
        res.status = 400;
        res.set_content(err.dump(), "application/json");
        return;
      }

      std::string name = j["name"].get<std::string>();
      std::string password = j["password"].get<std::string>();
      int age = j["age"].get<int>();
      double weightKg = j["weightKg"].get<double>();
      double heightM = j["heightM"].get<double>();
      std::string gender = j["gender"].get<std::string>();

      bool ok = backend.registerUser(name, age, weightKg, heightM, password, gender);
      if (!ok) {
        json err;
        err["errorMessage"] = "User already exists";
        res.status = 409;
        res.set_content(err.dump(), "application/json");
        return;
      }

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
      util::Logger::info(std::string("POST /register: user=") + name + " token=" + token);
    } catch (const std::exception& e) {
      json err;
      err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
      res.status = 400;
      res.set_content(err.dump(), "application/json");
    }
  });

  // POST /login
  // Body: { "name","password" }
  // 回傳: 200 { "token":"..." }
  svr.Post("/login", [&backend](const httplib::Request& req, httplib::Response& res) {
    try {
      json j = json::parse(req.body);

      if (!j.contains("name") || !j.contains("password")) {
        json err;
        err["errorMessage"] = "Missing name or password";
        res.status = 400;
        res.set_content(err.dump(), "application/json");
        return;
      }

      std::string name = j["name"].get<std::string>();
      std::string password = j["password"].get<std::string>();

      std::string token = backend.login(name, password);
      if (token == "INVALID") {
        json err;
        err["errorMessage"] = "Invalid name or password";
        res.status = 401;
        res.set_content(err.dump(), "application/json");
        util::Logger::warn(std::string("POST /login failed: user=") + name);
        return;
      }

      json out;
      out["token"] = token;
      res.status = 200;
      res.set_content(out.dump(), "application/json");
      util::Logger::info(std::string("POST /login: user=") + name + " token=" + token);
    } catch (const std::exception& e) {
      json err;
      err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
      res.status = 400;
      res.set_content(err.dump(), "application/json");
    }
  });

  // GET /user/profile
  svr.Get("/user/profile", [&backend](const httplib::Request& req, httplib::Response& res) {
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
    out["id"] = profile.id;
    out["name"] = profile.name;
    out["gender"] = profile.gender;
    out["weightKg"] = profile.weightKg;
    out["heightM"] = profile.heightM;
    out["age"] = profile.age;

    res.status = 200;
    res.set_content(out.dump(), "application/json");
  });

  // GET /user/bmi
  svr.Get("/user/bmi", [&backend](const httplib::Request& req, httplib::Response& res) {
    std::string token = getTokenFromAuthHeader(req);
    if (token.empty()) {
      json err;
      err["errorMessage"] = "Missing or invalid Authorization token";
      res.status = 401;
      res.set_content(err.dump(), "application/json");
      return;
    }

    double bmi = backend.getBMI(token);
    if (bmi <= 0.0) {
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

  svr.Post("/waters", [&backend](const httplib::Request& req, httplib::Response& res) {
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
      double amount = j["amountMl"].get<double>();

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
      const auto& r = records[idx];

      json out;
      out["id"] = std::to_string(idx);
      out["datetime"] = r.datetime;
      out["amountMl"] = r.amountMl;
      res.status = 201;
      res.set_content(out.dump(), "application/json");
    } catch (const std::exception& e) {
      json err;
      err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
      res.status = 400;
      res.set_content(err.dump(), "application/json");
    }
  });

  svr.Get("/waters", [&backend](const httplib::Request& req, httplib::Response& res) {
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
      const auto& r = records[i];
      json jr;
      jr["id"] = std::to_string(i);
      jr["datetime"] = r.datetime;
      jr["amountMl"] = r.amountMl;
      arr.push_back(jr);
    }

    res.status = 200;
    res.set_content(arr.dump(), "application/json");
  });

  svr.Patch(R"(/waters/(\d+))", [&backend](const httplib::Request& req, httplib::Response& res) {
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
      double newAmount = records[index].amountMl;

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
      out["id"] = idStr;
      out["datetime"] = newDatetime;
      out["amountMl"] = newAmount;
      res.status = 200;
      res.set_content(out.dump(), "application/json");
    } catch (const std::exception& e) {
      json err;
      err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
      res.status = 400;
      res.set_content(err.dump(), "application/json");
    }
  });

  svr.Delete(R"(/waters/(\d+))", [&backend](const httplib::Request& req, httplib::Response& res) {
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

    res.status = 204;
    res.set_content("", "application/json");
  });

  // =======================
  //         Sleeps
  // =======================

  svr.Post("/sleeps", [&backend](const httplib::Request& req, httplib::Response& res) {
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
      double hours = j["hours"].get<double>();

      bool ok = backend.addSleep(token, datetime, hours);
      if (!ok) {
        json err;
        err["errorMessage"] = "Failed to add sleep record";
        res.status = 400;
        res.set_content(err.dump(), "application/json");
        util::Logger::warn(std::string("POST /sleeps failed: token=") + token + " hours=" + std::to_string(hours));
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
      const auto& r = records[idx];

      json out;
      out["id"] = std::to_string(idx);
      out["datetime"] = r.datetime;
      out["hours"] = r.hours;
      res.status = 201;
      res.set_content(out.dump(), "application/json");
    } catch (const std::exception& e) {
      json err;
      err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
      res.status = 400;
      res.set_content(err.dump(), "application/json");
    }
  });

  svr.Get("/sleeps", [&backend](const httplib::Request& req, httplib::Response& res) {
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
      const auto& r = records[i];
      json jr;
      jr["id"] = std::to_string(i);
      jr["datetime"] = r.datetime;
      jr["hours"] = r.hours;
      arr.push_back(jr);
    }

    res.status = 200;
    res.set_content(arr.dump(), "application/json");
  });

  svr.Patch(R"(/sleeps/(\d+))", [&backend](const httplib::Request& req, httplib::Response& res) {
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
      double newHours = records[index].hours;

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
      out["id"] = idStr;
      out["datetime"] = newDatetime;
      out["hours"] = newHours;
      res.status = 200;
      res.set_content(out.dump(), "application/json");
    } catch (const std::exception& e) {
      json err;
      err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
      res.status = 400;
      res.set_content(err.dump(), "application/json");
    }
  });

  svr.Delete(R"(/sleeps/(\d+))", [&backend](const httplib::Request& req, httplib::Response& res) {
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

  svr.Post("/activities", [&backend](const httplib::Request& req, httplib::Response& res) {
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
      if (!j.contains("datetime") || !j.contains("minutes") || !j.contains("intensity")) {
        json err;
        err["errorMessage"] = "Missing fields";
        res.status = 400;
        res.set_content(err.dump(), "application/json");
        return;
      }

      std::string datetime = j["datetime"].get<std::string>();
      int minutes = j["minutes"].get<int>();
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
      const auto& a = records[idx];

      json out;
      out["id"] = std::to_string(idx);
      out["datetime"] = a.datetime;
      out["minutes"] = a.minutes;
      out["intensity"] = a.intensity;
      res.status = 201;
      res.set_content(out.dump(), "application/json");
    } catch (const std::exception& e) {
      json err;
      err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
      res.status = 400;
      res.set_content(err.dump(), "application/json");
    }
  });

  svr.Get("/activities", [&backend](const httplib::Request& req, httplib::Response& res) {
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
      const auto& a = records[i];
      json ja;
      ja["id"] = std::to_string(i);
      ja["datetime"] = a.datetime;
      ja["minutes"] = a.minutes;
      ja["intensity"] = a.intensity;
      arr.push_back(ja);
    }

    res.status = 200;
    res.set_content(arr.dump(), "application/json");
  });

  svr.Patch(R"(/activities/(\d+))", [&backend](const httplib::Request& req, httplib::Response& res) {
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

      std::string newDatetime = records[index].datetime;
      int newMinutes = records[index].minutes;
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
      out["id"] = idStr;
      out["datetime"] = newDatetime;
      out["minutes"] = newMinutes;
      out["intensity"] = newIntensity;
      res.status = 200;
      res.set_content(out.dump(), "application/json");
    } catch (const std::exception& e) {
      json err;
      err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
      res.status = 400;
      res.set_content(err.dump(), "application/json");
    }
  });

  svr.Delete(R"(/activities/(\d+))", [&backend](const httplib::Request& req, httplib::Response& res) {
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
  svr.Get("/category/list", [&backend](const httplib::Request& req, httplib::Response& res) {
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
    for (const auto& name : cats) {
      json jc;
      jc["id"] = name;
      jc["categoryName"] = name;
      arr.push_back(jc);
    }

    res.status = 200;
    res.set_content(arr.dump(), "application/json");
  });

  // ===== CHANGED: /category/create 會呼叫 backend.createCategory =====
  // POST /category/create
  svr.Post("/category/create", [&backend](const httplib::Request& req, httplib::Response& res) {
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
      if (!j.contains("categoryName")) {
        json err;
        err["errorMessage"] = "Missing categoryName";
        res.status = 400;
        res.set_content(err.dump(), "application/json");
        return;
      }

      std::string name = j["categoryName"].get<std::string>();

      bool ok = backend.createCategory(token, name);
      if (!ok) {
        json err;
        err["errorMessage"] = "Category already exists or invalid name";
        res.status = 400;
        res.set_content(err.dump(), "application/json");
        return;
      }

      json out;
      out["id"] = name;
      out["categoryName"] = name;
      res.status = 201;
      res.set_content(out.dump(), "application/json");
    } catch (const std::exception& e) {
      json err;
      err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
      res.status = 400;
      res.set_content(err.dump(), "application/json");
    }
  });

  // ===== NEW: DELETE 整個 category =====
  // DELETE /category/{categoryId}
  svr.Delete(R"(/category/([^/]+)$)", [&backend](const httplib::Request& req, httplib::Response& res) {
    std::string token = getTokenFromAuthHeader(req);
    if (token.empty()) {
      json err;
      err["errorMessage"] = "Missing or invalid Authorization token";
      res.status = 401;
      res.set_content(err.dump(), "application/json");
      return;
    }

    std::string categoryId = req.matches[1];

    bool ok = backend.deleteCategory(token, categoryId);
    if (!ok) {
      json err;
      err["errorMessage"] = "Category not found";
      res.status = 404;
      res.set_content(err.dump(), "application/json");
      return;
    }

    res.status = 204;
    res.set_content("", "application/json");
  });

  // GET /category/{categoryId}/list
  svr.Get(R"(/category/([^/]+)/list)", [&backend](const httplib::Request& req, httplib::Response& res) {
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
      const auto& r = records[i];
      json jr;
      jr["id"] = std::to_string(i);
      jr["datetime"] = r.datetime;
      jr["note"] = r.note;
      arr.push_back(jr);
    }

    res.status = 200;
    res.set_content(arr.dump(), "application/json");
  });

  // POST /category/{categoryId}/add
  svr.Post(R"(/category/([^/]+)/add)", [&backend](const httplib::Request& req, httplib::Response& res) {
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
      std::string note = j["note"].get<std::string>();

      bool ok = backend.addOtherRecord(token, categoryId, datetime, 0.0, note);
      if (!ok) {
        json err;
        err["errorMessage"] = "Category not found or invalid data";
        res.status = 400;
        res.set_content(err.dump(), "application/json");
        return;
      }

      auto records = backend.getOtherRecords(token, categoryId);
      std::size_t idx = records.size() - 1;
      const auto& r = records[idx];

      json out;
      out["id"] = std::to_string(idx);
      out["categoryId"] = categoryId;
      out["datetime"] = r.datetime;
      out["note"] = r.note;
      res.status = 201;
      res.set_content(out.dump(), "application/json");
    } catch (const std::exception& e) {
      json err;
      err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
      res.status = 400;
      res.set_content(err.dump(), "application/json");
    }
  });

  // PATCH /category/{categoryId}/{itemId}
  svr.Patch(R"(/category/([^/]+)/([^/]+))", [&backend](const httplib::Request& req, httplib::Response& res) {
    std::string token = getTokenFromAuthHeader(req);
    if (token.empty()) {
      json err;
      err["errorMessage"] = "Missing or invalid Authorization token";
      res.status = 401;
      res.set_content(err.dump(), "application/json");
      return;
    }

    std::string categoryId = req.matches[1];
    std::string itemIdStr = req.matches[2];
    std::size_t index = 0;

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
      std::string newNote = records[index].note;
      double value = records[index].value;

      if (j.contains("datetime")) {
        newDatetime = j["datetime"].get<std::string>();
      }
      if (j.contains("note")) {
        newNote = j["note"].get<std::string>();
      }

      bool ok = backend.updateOtherRecord(token, categoryId, index, newDatetime, value, newNote);
      if (!ok) {
        json err;
        err["errorMessage"] = "Failed to update category item";
        res.status = 400;
        res.set_content(err.dump(), "application/json");
        return;
      }

      json out;
      out["id"] = std::to_string(index);
      out["categoryId"] = categoryId;
      out["datetime"] = newDatetime;
      out["note"] = newNote;
      res.status = 200;
      res.set_content(out.dump(), "application/json");
    } catch (const std::exception& e) {
      json err;
      err["errorMessage"] = std::string("Invalid JSON: ") + e.what();
      res.status = 400;
      res.set_content(err.dump(), "application/json");
    }
  });

  // DELETE /category/{categoryId}/{itemId}
  svr.Delete(R"(/category/([^/]+)/([^/]+))", [&backend](const httplib::Request& req, httplib::Response& res) {
    std::string token = getTokenFromAuthHeader(req);
    if (token.empty()) {
      json err;
      err["errorMessage"] = "Missing or invalid Authorization token";
      res.status = 401;
      res.set_content(err.dump(), "application/json");
      return;
    }

    std::string categoryId = req.matches[1];
    std::string itemIdStr = req.matches[2];
    std::size_t index = 0;

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

  util::Logger::info("Server started at http://0.0.0.0:8080");
  svr.listen("0.0.0.0", 8080);

  util::Logger::shutdown();

  return 0;
}