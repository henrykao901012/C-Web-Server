# C Web Server

一個輕量級的跨平台 C 語言 Web Server，支援靜態檔案服務和 RESTful API 框架兩種模式。

## 🚀 特性

### 核心功能
- 🌐 跨平台支援（Windows/Linux/macOS）
- 🔧 多執行緒處理
- 📝 完整的日誌系統
- 🔒 安全的路徑處理
- 📦 模組化設計

### 兩種運行模式

#### 1. 靜態檔案伺服器 (webserver.exe)
- 提供靜態檔案服務（HTML、CSS、JS、圖片等）
- 自動識別檔案 MIME 類型
- 適用於網頁託管、文件展示

#### 2. API 框架模式 (webapi.exe)
- RESTful API 支援
- 動態路由系統（支援路徑參數如 `/api/users/:id`）
- JSON 資料處理
- CORS 支援
- 適用於後端開發、微服務

## 📋 系統需求

- GCC 編譯器
- Windows: MinGW 或 TDM-GCC
- Linux/macOS: 預設 GCC

## 🛠️ 快速開始

### 步驟 1：編譯 build 工具

```bash
# Windows
gcc build.c -o build.exe

# Linux/macOS
gcc build.c -o build
```

### 步驟 2：選擇編譯模式

```bash
# 編譯靜態檔案伺服器
build              # Windows: build.exe

# 編譯 API 框架
build framework    # Windows: build.exe framework

# 清理編譯檔案
build clean        # Windows: build.exe clean

# 查看幫助
build help         # Windows: build.exe help
```

### 步驟 3：執行伺服器

```bash
# 靜態檔案伺服器（預設埠 8080）
./webserver        # Windows: webserver.exe

# API 框架伺服器（預設埠 8080）
./webapi           # Windows: webapi.exe

# 指定埠號
./webserver 3000   # Windows: webserver.exe 3000
./webapi 3000      # Windows: webapi.exe 3000
```

## 📁 專案結構

project/
├── build.c
├── README.md
├── core/
│   ├── server.c
│   ├── server.h
│   ├── file_utils.c
│   ├── file_utils.h
│   ├── logger.c
│   ├── logger.h
│   └── http_handler.h      ← 移到這裡
├── static_server/
│   ├── static_server.c
│   └── http_handler_static.c
├── api_framework/
│   ├── example_app.c
│   ├── http_handler_api.c
│   ├── router.c
│   ├── router.h
│   ├── json.c
│   └── json.h
└── www/
    └── index.html

## 💡 使用範例

### 靜態檔案伺服器

將網頁檔案放在 `www` 目錄下：
```
www/
├── index.html
├── style.css
├── script.js
└── images/
    └── logo.png
```

訪問：`http://localhost:8080/index.html`

### API 框架

內建範例 API：
```bash
# 取得伺服器時間
GET http://localhost:8080/api/time

# 取得所有使用者
GET http://localhost:8080/api/users

# 取得特定使用者
GET http://localhost:8080/api/users/1

# 建立新使用者
POST http://localhost:8080/api/users
Content-Type: application/json
{
  "name": "John",
  "email": "john@example.com"
}
```

## 🔧 開發自己的 API

編輯 `example_app.c`，加入新的路由：

```c
// 定義處理函數
void my_api_handler(Request* req, Response* res) {
    JsonBuilder* json = json_create();
    json_add_string(json, "message", "Hello World!");
    json_add_number(json, "code", 200);
    
    set_json_response(res, 200, json_get_string(json));
    json_destroy(json);
}

// 在 main 函數中註冊路由
router_add(HTTP_GET, "/api/hello", my_api_handler);
```

## 📊 模式對比

| 特性 | 靜態檔案伺服器 | API 框架 |
|------|----------------|----------|
| 執行檔 | webserver.exe | webapi.exe |
| HTTP 方法 | GET only | GET/POST/PUT/DELETE |
| 內容類型 | 靜態檔案 | 動態生成 |
| 路由系統 | 檔案路徑 | 程式定義 |
| 適用場景 | 網頁託管 | 後端服務 |

## ⚙️ 配置選項

### 預設設定
- 預設埠：8080
- 日誌檔案：server.log
- 緩衝區大小：4096 bytes
- 最大連線數：100

### 修改設定
編輯 `server.h` 中的定義：
```c
#define DEFAULT_PORT 8080
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 100
```

## 🐛 除錯

1. **檢查日誌檔案**
   - 所有請求和錯誤都記錄在 `server.log`

2. **常見問題**
   - 埠被佔用：更換埠號或結束佔用程式
   - 防火牆阻擋：允許程式通過防火牆
   - 找不到檔案：確認檔案在 `www` 目錄下（靜態模式）

3. **測試工具**
   - 瀏覽器：直接訪問 URL
   - curl：命令列測試 API
   - Postman：圖形化 API 測試

