# Health Backend (C++17)

A simple health tracking backend implemented in C++17, with:
	•	User management (register / login / token)
	•	BMI calculation
	•	Water intake records (daily records, weekly average, enough or not)
	•	Sleep records (last sleep, enough or not)
	•	Activity records (add, edit, delete, sort)
	•	Custom categories (create, add record, edit, delete)
	•	HTTP API server built on top of the core backend
	•	Full persistent storage using storage.json

本專案將「核心健康資料邏輯」與「HTTP 通訊層」分開設計：
	•	main.cpp：單純測試後端邏輯（不含 HTTP）
	•	server.cpp：提供 HTTP API，讓前端或其他 client 呼叫
	•	/backend, /user, /records, /helpers：核心 C++ 類別與工具
	•	/data/storage.json：所有資料持久化存檔（自動讀寫）

---

## Folder Structure

health_backend/
├── main.cpp                     # Core backend testing (no HTTP)
├── server.cpp                   # HTTP server entry point (REST API)
├── httplib.h                    # cpp-httplib (header-only HTTP library)
│
├── external/
│   └── json.hpp                 # nlohmann JSON header-only library
│
├── backend/
│   ├── HealthBackend.hpp
│   └── HealthBackend.cpp
│
├── user/
│   ├── User.hpp
│   ├── User.cpp
│   ├── UserBackend.hpp
│   └── UserBackend.cpp
│
├── records/
│   ├── Water.hpp
│   ├── Water.cpp
│   ├── Sleep.hpp
│   ├── Sleep.cpp
│   ├── Activity.hpp
│   ├── Activity.cpp
│   ├── OtherCategory.hpp
│   └── OtherCategory.cpp
│
├── helpers/
│   ├── validation.hpp
│   ├── validation.cpp
│   ├── json.hpp                 # (replaced by nlohmann/json)
│
└── data/
    ├── storage.json             # Auto-generated persistent storage
    └── storage.example.json     # Example layout (no real data)

---

Build Instructions

Make sure you are inside the project folder:

cd health_backend

Compile the HTTP server:

g++ -std=c++17 \
  server.cpp \
  backend/HealthBackend.cpp \
  user/User.cpp \
  user/UserBackend.cpp \
  records/Water.cpp \
  records/Sleep.cpp \
  records/Activity.cpp \
  records/OtherCategory.cpp \
  helpers/validation.cpp \
  -o server_app


---

Run the Server

./server_app

預期輸出：

Server started at http://0.0.0.0:8080

你可在瀏覽器開：

http://localhost:8080

若要同 Wi-Fi 前端電腦訪問：

http://<你的IP>:8080


---

## Persistent Storage

所有資料會自動存入：

/data/storage.json

	•	伺服器啟動：自動讀取
	•	有任何新增／修改／刪除：自動寫回
	•	刪除檔案 → 重置所有資料

---

## API Authentication

登入取得 token。

所有 API（除了 register/login）都必須加上：

X-Auth-Token: <token>


---

## API Endpoints Overview

### User APIs

| Method | Endpoint     | Description        | Auth          |
|--------|-------------|--------------------|---------------|
| POST   | `/register` | Register new user  | ❌            |
| POST   | `/login`    | Login and get token| ❌            |
| GET    | `/user/bmi` | Get BMI            | ✅ X-Auth-Token |

---

### Water APIs

| Method | Endpoint                    | Description                          | Auth |
|--------|-----------------------------|--------------------------------------|------|
| POST   | `/water/add`                | Add a water record                   | ✅   |
| POST   | `/water/edit`               | Edit a water record (by index)       | ✅   |
| POST   | `/water/delete`             | Delete a water record (by index)     | ✅   |
| GET    | `/water/all`                | Get all water records                | ✅   |
| GET    | `/water/weekly_average`     | Get weekly average (last up to 7)    | ✅   |
| GET    | `/water/is_enough?goal=1500`| Check if weekly avg ≥ goal (ml/day)  | ✅   |

---

### Sleep APIs

| Method | Endpoint                      | Description                          | Auth |
|--------|-------------------------------|--------------------------------------|------|
| POST   | `/sleep/add`                  | Add a sleep record                   | ✅   |
| POST   | `/sleep/edit`                 | Edit a sleep record (by index)       | ✅   |
| POST   | `/sleep/delete`               | Delete a sleep record (by index)     | ✅   |
| GET    | `/sleep/all`                  | Get all sleep records                | ✅   |
| GET    | `/sleep/last_hours`           | Get last sleep hours                 | ✅   |
| GET    | `/sleep/is_enough?min=7`      | Check if last sleep ≥ min hours      | ✅   |

---

### Activity APIs

| Method | Endpoint                        | Description                          | Auth |
|--------|----------------------------------|--------------------------------------|------|
| POST   | `/activity/add`                 | Add an activity record               | ✅   |
| POST   | `/activity/edit`                | Edit an activity record (by index)   | ✅   |
| POST   | `/activity/delete`              | Delete an activity record (by index) | ✅   |
| GET    | `/activity/all`                 | Get all activity records             | ✅   |
| GET    | `/activity/sort_by_duration`    | Sort activities by duration (desc)   | ✅   |

---

### Other Category APIs

| Method | Endpoint                                   | Description                                | Auth |
|--------|--------------------------------------------|--------------------------------------------|------|
| POST   | `/other/create`                           | Create a new category                      | ✅   |
| POST   | `/other/add_record`                       | Add record to a category (by name + index) | ✅   |
| POST   | `/other/edit_record`                      | Edit a record in a category                | ✅   |
| POST   | `/other/delete_record`                    | Delete a record in a category              | ✅   |
| GET    | `/other/categories`                       | List all category names                    | ✅   |
| GET    | `/other/get_records?category=<name>`      | Get all records in a category              | ✅   |

---

Example Requests (curl)

Register

curl -X POST http://localhost:8080/register \
  -H "Content-Type: application/json" \
  -d '{"name":"Alice","age":20,"weightKg":50,"heightM":1.60,"password":"abc123"}'

Login

curl -X POST http://localhost:8080/login \
  -H "Content-Type: application/json" \
  -d '{"name":"Alice","password":"abc123"}'

BMI

curl http://localhost:8080/user/bmi \
  -H "X-Auth-Token: <token>"


---

Notes
	•	main.cpp 用於純後端邏輯測試，不提供 HTTP。
	•	server.cpp 才是提供 REST API 的入口。
	•	資料儲存位置：data/storage.json。
	•	JSON parser 使用 nlohmann/json（header-only）。



