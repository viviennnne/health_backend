# Health Backend (C++17)

A simple health tracking backend implemented in C++17, with:

- User management (register / login / token)
- BMI calculation
- Water intake records (daily records, weekly average, enough or not)
- Sleep records (last sleep, enough or not)
- Activity records (add, edit, delete, sort)
- Custom categories (create, add record, edit, delete)
- HTTP API server built on top of the core backend
- Full persistent storage using `data/storage.json`

本專案將「核心健康資料邏輯」與「HTTP 通訊層」分開設計：

- `main.cpp`：單純測試後端邏輯（不含 HTTP）
- `server.cpp`：提供 HTTP API，讓前端或其他 client 呼叫
- `backend`, `user`, `records`, `helpers`：核心 C++ 類別與工具
- `data/storage.json`：所有資料持久化存檔（自動讀寫）

---

## Folder Structure

```
health_backend/
├── main.cpp                    # Core backend testing (no HTTP)
├── server.cpp                  # HTTP server entry point (REST API)
├── httplib.h                   # cpp-httplib (header-only HTTP library)
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
│   └── json.hpp                 # (replaced by nlohmann/json)
│
└── data/
    ├── storage.json             # Auto-generated persistent storage
    └── storage.example.json     # Example layout (no real data)
```

---

## Build Instructions

Make sure you are inside the project folder:

```bash
cd health_backend
```

Compile the HTTP server:

```bash
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
  helpers/Logger.cpp \
  -o server_app
```

---

## Run the Server

Start the server:

```bash
./server_app
```

Expected output:

```
Server started at http://0.0.0.0:8080

Logs are written to `logs/server.log` by default. You can change the behaviour by setting environment variables before starting the server:

- `LOG_FILE`: path to the log file (default `logs/server.log`)
- `LOG_LEVEL`: `DEBUG`, `INFO`, `WARN`, or `ERROR` (default `INFO`)

Example:

```bash
LOG_FILE="/var/log/health_backend.log" LOG_LEVEL=DEBUG ./server_app
```
```

Open in browser:

- http://localhost:8080
- Or replace localhost with your machine IP: `http://<your-ip>:8080`

---

## Persistent Storage

All data is stored in `data/storage.json`. The behavior is:

- On server start: loads `data/storage.json` (if present).
- On create/update/delete: automatically writes to `data/storage.json`.
- Delete the file to reset all data.

---

## API Authentication

After registering or logging in, you receive a token. All API requests (except `/register` and `/login`) require the `Authorization` header with a Bearer token, e.g.:

```
Authorization: Bearer <token>
```

Example curl:

```bash
curl -H "Authorization: Bearer <token>" http://localhost:8080/user/profile
```

---

## CORS

This server adds CORS headers and responds to preflight `OPTIONS` requests. By default it sets `Access-Control-Allow-Origin: *`. If you need a more restricted origin, update the server code in `server.cpp`.

---

## Notes

- `main.cpp` is for backend logic testing (no HTTP).
- `server.cpp` is the REST API entry point.
- Data is persisted to `data/storage.json`.
- JSON parsing is implemented with `nlohmann/json` (header-only).
