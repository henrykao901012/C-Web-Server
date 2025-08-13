# 🌐 自製 Ngrok 隧道系統部署指南

## 系統架構

```
[本地Web服務] <-> [隧道客戶端] <==網路==> [隧道服務器] <-> [公網用戶]
 localhost:8080     你的電腦              VPS服務器        互聯網
```

## 📦 文件結構

```
tunnel_system/
├── tunnel_common.h      # 協議定義
├── tunnel_common.c      # 協議實現
├── tunnel_client.c      # 客戶端程序
├── tunnel_server.c      # 服務器程序
├── core/
│   ├── logger.h        # 日誌系統
│   └── logger.c
└── Makefile
```

## 🚀 快速部署

### Step 1: 編譯

```bash
# 編譯所有
make

# 只編譯客戶端（本地使用）
make client

# 只編譯服務器（VPS使用）
make server
```

### Step 2: 部署服務器（在VPS上）

#### 2.1 準備VPS
- 需要一個有公網IP的VPS（如 DigitalOcean, Linode, AWS EC2）
- 系統：Ubuntu/Debian/CentOS
- 開放端口：80, 7000, 7001

#### 2.2 在VPS上編譯和運行

```bash
# SSH連接到VPS
ssh root@your-vps-ip

# 安裝依賴
apt-get update
apt-get install gcc make git

# 下載代碼
git clone your-repo
cd tunnel_system

# 編譯服務器
make server

# 運行服務器（建議用screen或systemd）
./tunnel_server

# 或使用screen保持後台運行
screen -S tunnel
./tunnel_server
# Ctrl+A+D 分離screen
```

#### 2.3 配置域名（可選）

如果你有域名，配置DNS：
```
*.tunnel.yourdomain.com -> VPS_IP
```

或使用 xip.io 服務：
```
http://subdomain.VPS_IP.xip.io
```

### Step 3: 運行客戶端（本地）

```bash
# 1. 啟動你的Web服務
./webserver.exe 8080

# 2. 啟動隧道客戶端
./tunnel_client.exe vps-ip 7000 8080

# 輸出示例：
# ========================================
#   Tunnel established successfully!
#   Public URL: http://abc123.tunnel.example.com
#   Forwarding to: localhost:8080
# ========================================
```

## 🔧 高級配置

### 使用 systemd 管理服務器（推薦）

創建 `/etc/systemd/system/tunnel-server.service`:

```ini
[Unit]
Description=Tunnel Server
After=network.target

[Service]
Type=simple
User=tunnel
WorkingDirectory=/opt/tunnel
ExecStart=/opt/tunnel/tunnel_server
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

啟動服務：
```bash
systemctl enable tunnel-server
systemctl start tunnel-server
systemctl status tunnel-server
```

### 使用 Nginx 反向代理（可選）

如果你想使用標準80端口並支持HTTPS：

```nginx
server {
    listen 80;
    server_name *.tunnel.yourdomain.com;
    
    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # WebSocket支持
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

### 添加認證功能

修改 `tunnel_server.c` 添加token驗證：

```c
// 在 handle_control_connection 函數中
if (strcmp(req->token, "your-secret-token") != 0) {
    tunnel_send_msg(client_sock, TUNNEL_REJECT, 0, NULL, 0);
    close(client_sock);
    return NULL;
}
```

## 📊 監控和日誌

### 查看服務器日誌
```bash
tail -f tunnel_server.log
```

### 查看客戶端日誌
```bash
tail -f tunnel_client.log
```

### 監控連接狀態
```bash
# 查看端口監聽
netstat -tlnp | grep tunnel

# 查看活動連接
ss -tnp | grep :7000
```

## 🛠️ 故障排查

### 問題 1: 無法連接到服務器
```bash
# 檢查防火牆
ufw allow 80/tcp
ufw allow 7000/tcp
ufw allow 7001/tcp

# 或關閉防火牆測試
ufw disable
```

### 問題 2: 域名無法訪問
```bash
# 檢查DNS解析
nslookup subdomain.tunnel.yourdomain.com

# 使用IP直接訪問測試
curl http://VPS_IP
```

### 問題 3: 連接不穩定
- 增加心跳頻率
- 調整超時時間
- 使用更穩定的VPS提供商

## 🎯 性能優化

### 1. 增加緩衝區大小
```c
#define TUNNEL_BUFFER_SIZE 131072  // 128KB
```

### 2. 使用多線程池
```c
// 預創建線程池處理連接
ThreadPool* pool = thread_pool_create(100);
```

### 3. 啟用TCP優化
```c
int flag = 1;
setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
```

## 📈 擴展功能

### 支持的擴展
- ✅ 多客戶端支持
- ✅ 自定義子域名
- ✅ 心跳保活
- ✅ 會話管理

### 可以添加的功能
- [ ] HTTPS支持
- [ ] WebSocket代理
- [ ] 流量統計
- [ ] 帶寬限制
- [ ] 多端口轉發
- [ ] UDP隧道
- [ ] 壓縮傳輸
- [ ] 加密通道

## 🔐 安全建議

1. **使用Token認證**
   ```bash
   ./tunnel_client vps-ip 7000 8080 secret-token
   ```

2. **限制來源IP**
   ```c
   // 只允許特定IP連接
   if (strcmp(inet_ntoa(client_addr.sin_addr), "allowed-ip") != 0) {
       close(client_sock);
       return;
   }
   ```

3. **使用TLS加密**
   - 可以整合 OpenSSL 或 mbedTLS

4. **限制資源使用**
   - 限制最大連接數
   - 限制帶寬使用
   - 設置超時時間

## 📝 完整使用示例

### 本地開發環境
```bash
# 1. VPS上運行服務器
ssh root@vps-ip
./tunnel_server

# 2. 本地運行Web服務
./webserver.exe 3000

# 3. 創建隧道
./tunnel_client.exe vps-ip 7000 3000

# 4. 分享公網地址
# http://abc123.vps-ip.xip.io
```

### 生產環境部署
```bash
# 使用supervisor管理
[program:tunnel-server]
command=/opt/tunnel/tunnel_server
autostart=true
autorestart=true
stderr_logfile=/var/log/tunnel.err.log
stdout_logfile=/var/log/tunnel.out.log
```

## 🎉 成功標誌

當你看到以下內容，表示系統運行成功：

**服務器端：**
```
========================================
  Tunnel Server Starting...
  HTTP Port: 80
  Control Port: 7000
  Data Port: 7001
========================================
[INFO] HTTP server listening on port 80
[INFO] Control server listening on port 7000
[INFO] Client connected: default -> http://abc123.tunnel.example.com
```

**客戶端：**
```
========================================
  Tunnel established successfully!
  Public URL: http://abc123.tunnel.example.com
  Forwarding to: localhost:8080
========================================
```

現在你就有了自己的 ngrok！訪問分配的公網URL即可訪問你的本地服務。