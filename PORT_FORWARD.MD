# Port Forwarding Module 使用說明

## 🚀 快速開始

### 1. 編譯
```bash
# Linux/macOS
make

# Windows (使用 MinGW)
mingw32-make

# 或直接使用 gcc
gcc -Wall -O2 -pthread port_forward/forward_cli.c port_forward/port_forward.c core/logger.c -o portforward -lpthread
# Windows 需要加上 -lws2_32
```

### 2. 運行
```bash
# 進入交互式模式
./portforward

# 使用配置文件啟動
./portforward forward.conf
```

## 📁 文件結構

請確保你的專案有以下結構：
```
your-project/
├── Makefile
├── core/
│   ├── logger.h
│   └── logger.c
├── port_forward/
│   ├── port_forward.h
│   ├── port_forward.c
│   └── forward_cli.c
└── forward.conf (選用)
```

## 💻 交互式命令

進入程式後，可使用以下命令：

### 添加轉發規則
```
> add 8080 www.google.com 80
> add 3306 192.168.1.100 3306 MySQL數據庫轉發
```

### 查看所有規則
```
> list
```

### 啟動/停止服務
```
> start    # 啟動所有轉發規則
> stop     # 停止所有轉發規則
```

### 啟用/禁用特定規則
```
> enable 1   # 啟用規則 ID 1
> disable 2  # 禁用規則 ID 2
```

### 刪除規則
```
> remove 1   # 刪除規則 ID 1
```

### 配置文件操作
```
> save              # 保存到 forward.conf
> save myconfig.txt # 保存到指定文件
> load              # 從 forward.conf 載入
> load myconfig.txt # 從指定文件載入
```

### 其他
```
> help   # 顯示幫助
> quit   # 退出程式
```

## 📋 配置文件格式

`forward.conf` 文件格式：
```
# 註釋以 # 開頭
# 格式: listen_port target_host target_port [描述]

8080 www.google.com 80 Google HTTP
8443 www.google.com 443 Google HTTPS
3306 192.168.1.100 3306 MySQL Database
6379 192.168.1.101 6379 Redis Cache
```

## 📚 使用範例

### 範例 1: HTTP 代理
```bash
./portforward
> add 8080 www.example.com 80 網站代理
> start
# 現在訪問 http://localhost:8080 會轉發到 www.example.com
```

### 範例 2: 數據庫轉發
```bash
./portforward
> add 3307 remote-db.server.com 3306 遠程MySQL
> start
# 現在可以通過 localhost:3307 連接遠程數據庫
```

### 範例 3: 多個服務轉發
```bash
# 創建配置文件 services.conf
cat > services.conf << EOF
8080 backend1.local 80 Backend Server 1
8081 backend2.local 80 Backend Server 2
3306 db.local 3306 Database
6379 cache.local 6379 Redis
EOF

# 使用配置文件啟動
./portforward services.conf
> start
```

## 🔧 Makefile 目標

```bash
make          # 編譯程式
make clean    # 清理編譯文件
make run      # 編譯並運行
make run-config # 使用 forward.conf 運行
make help     # 顯示幫助
```

## ⚠️ 注意事項

1. **端口權限**: 綁定小於 1024 的端口需要管理員權限
   ```bash
   sudo ./portforward  # Linux/macOS
   ```

2. **防火牆**: 確保防火牆允許你要監聽的端口

3. **端口衝突**: 確保監聽端口沒有被其他程式使用

4. **日誌文件**: 程式會創建 `server.log` 記錄所有活動

## 🐛 故障排除

### 端口已被佔用
```
Error: Failed to bind to port 8080
解決: 更換端口或停止佔用該端口的程式
```

### 無法連接到目標
```
Error: Failed to connect to target.com:80
解決: 檢查目標主機是否可達，DNS 是否正確
```

### Windows 下編譯錯誤
```
確保使用 MinGW 或 Cygwin，並包含 -lws2_32 鏈接選項
```

## 📝 完整使用流程

```bash
# 1. 編譯
make

# 2. 創建配置文件
cat > my_forwards.conf << EOF
8080 www.google.com 80 Google
8081 www.github.com 80 GitHub
EOF

# 3. 運行
./portforward my_forwards.conf

# 4. 在程式中
> list     # 查看載入的規則
> start    # 啟動服務
> quit     # 退出
```

## 🎯 性能說明

- 每個連接使用獨立線程處理
- 支援最多 10 個同時轉發規則（可在 port_forward.h 中修改 MAX_FORWARD_RULES）
- 緩衝區大小為 8KB（可在 port_forward.h 中修改 FORWARD_BUFFER_SIZE）

## 📄 許可證

此模組是你的 C Web Server 專案的一部分，遵循相同的許可證。