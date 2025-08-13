# Makefile for Tunnel System
# 目錄結構:
# project/
#   ├── core/
#   │   ├── logger.h
#   │   └── logger.c
#   ├── tunnel/
#   │   ├── tunnel_common.h
#   │   ├── tunnel_common.c
#   │   ├── tunnel_client.c
#   │   ├── tunnel_server.c
#   │   └── Makefile (this file)

CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = 

# 偵測作業系統
ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32 -lpthread
    CLIENT_TARGET = tunnel_client.exe
    SERVER_TARGET = tunnel_server.exe
    RM = del /F /Q 2>nul || echo.
else
    CFLAGS += -pthread
    LDFLAGS += -pthread
    CLIENT_TARGET = tunnel_client
    SERVER_TARGET = tunnel_server
    RM = rm -f
endif

# 源文件路徑
TUNNEL_DIR = .
CORE_DIR = ../core

# 頭文件目錄 - 重要：這裡加入所有需要的 include 路徑
INCLUDES = -I. -I../core -I..

# 目標文件
COMMON_OBJS = tunnel_common.o logger.o
CLIENT_OBJS = tunnel_client.o $(COMMON_OBJS)
SERVER_OBJS = tunnel_server.o $(COMMON_OBJS)

# 預設目標
all: $(CLIENT_TARGET) $(SERVER_TARGET)
	@echo ================================
	@echo Build complete!
	@echo Client: $(CLIENT_TARGET)
	@echo Server: $(SERVER_TARGET)
	@echo ================================

# 只編譯客戶端
client: $(CLIENT_TARGET)
	@echo ================================
	@echo Client built: $(CLIENT_TARGET)
	@echo ================================

# 只編譯服務器
server: $(SERVER_TARGET)
	@echo ================================
	@echo Server built: $(SERVER_TARGET)
	@echo ================================

# 編譯客戶端執行檔
$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o $(CLIENT_TARGET) $(LDFLAGS)
	@echo Cleaning object files...
ifeq ($(OS),Windows_NT)
	@del /F /Q *.o 2>nul || echo.
else
	@rm -f *.o
endif

# 編譯服務器執行檔
$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o $(SERVER_TARGET) $(LDFLAGS)
	@echo Cleaning object files...
ifeq ($(OS),Windows_NT)
	@del /F /Q *.o 2>nul || echo.
else
	@rm -f *.o
endif

# 編譯規則
tunnel_client.o: $(TUNNEL_DIR)/tunnel_client.c $(TUNNEL_DIR)/tunnel_common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(TUNNEL_DIR)/tunnel_client.c -o tunnel_client.o

tunnel_server.o: $(TUNNEL_DIR)/tunnel_server.c $(TUNNEL_DIR)/tunnel_common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(TUNNEL_DIR)/tunnel_server.c -o tunnel_server.o

tunnel_common.o: $(TUNNEL_DIR)/tunnel_common.c $(TUNNEL_DIR)/tunnel_common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(TUNNEL_DIR)/tunnel_common.c -o tunnel_common.o

logger.o: $(CORE_DIR)/logger.c $(CORE_DIR)/logger.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(CORE_DIR)/logger.c -o logger.o

# 清理
clean:
ifeq ($(OS),Windows_NT)
	@del /F /Q *.o $(CLIENT_TARGET) $(SERVER_TARGET) *.log 2>nul || echo Clean complete
else
	@rm -f *.o $(CLIENT_TARGET) $(SERVER_TARGET) *.log
endif

# 深度清理
cleanall: clean

# 測試本地運行
test-local:
	@echo ================================
	@echo Local Test Instructions:
	@echo ================================
	@echo 1. Start your web server:
	@echo    ../webserver 8080
	@echo.
	@echo 2. Start tunnel server:
	@echo    $(SERVER_TARGET)
	@echo.
	@echo 3. Start tunnel client:
	@echo    $(CLIENT_TARGET) localhost 7000 8080
	@echo.
	@echo 4. Access through tunnel:
	@echo    http://localhost
	@echo ================================

# 執行客戶端
run-client:
	@echo Starting tunnel client...
	@echo Usage: $(CLIENT_TARGET) [server_host] [server_port] [local_port]
	@echo Example: $(CLIENT_TARGET) vps.example.com 7000 8080
	@echo.
	./$(CLIENT_TARGET) localhost 7000 8080

# 執行服務器
run-server:
	@echo Starting tunnel server...
	./$(SERVER_TARGET)

# 顯示幫助
help:
	@echo ================================
	@echo Tunnel System Makefile
	@echo ================================
	@echo Directory Structure Expected:
	@echo   project/
	@echo     - core/
	@echo       - logger.h
	@echo       - logger.c
	@echo     - tunnel/
	@echo       - tunnel_common.h
	@echo       - tunnel_common.c
	@echo       - tunnel_client.c
	@echo       - tunnel_server.c
	@echo       - Makefile
	@echo.
	@echo Available targets:
	@echo   make          - Build both client and server
	@echo   make client   - Build client only
	@echo   make server   - Build server only
	@echo   make clean    - Clean all build files
	@echo   make test-local - Show local test instructions
	@echo   make run-client - Run client (localhost test)
	@echo   make run-server - Run server
	@echo   make help     - Show this help
	@echo.
	@echo Usage Examples:
	@echo   Server (on VPS): make server && ./$(SERVER_TARGET)
	@echo   Client (local):  make client && ./$(CLIENT_TARGET) vps-ip 7000 8080
	@echo ================================

# 檢查環境
check:
	@echo Checking environment...
	@echo Current directory: $(shell pwd)
	@echo.
	@echo Checking core directory:
ifeq ($(OS),Windows_NT)
	@if exist $(CORE_DIR)\logger.c (echo [OK] $(CORE_DIR)\logger.c) else (echo [MISSING] $(CORE_DIR)\logger.c)
	@if exist $(CORE_DIR)\logger.h (echo [OK] $(CORE_DIR)\logger.h) else (echo [MISSING] $(CORE_DIR)\logger.h)
else
	@test -f $(CORE_DIR)/logger.c && echo "[OK] $(CORE_DIR)/logger.c" || echo "[MISSING] $(CORE_DIR)/logger.c"
	@test -f $(CORE_DIR)/logger.h && echo "[OK] $(CORE_DIR)/logger.h" || echo "[MISSING] $(CORE_DIR)/logger.h"
endif
	@echo.
	@echo Checking tunnel files:
ifeq ($(OS),Windows_NT)
	@if exist tunnel_common.c (echo [OK] tunnel_common.c) else (echo [MISSING] tunnel_common.c)
	@if exist tunnel_common.h (echo [OK] tunnel_common.h) else (echo [MISSING] tunnel_common.h)
	@if exist tunnel_client.c (echo [OK] tunnel_client.c) else (echo [MISSING] tunnel_client.c)
	@if exist tunnel_server.c (echo [OK] tunnel_server.c) else (echo [MISSING] tunnel_server.c)
else
	@test -f tunnel_common.c && echo "[OK] tunnel_common.c" || echo "[MISSING] tunnel_common.c"
	@test -f tunnel_common.h && echo "[OK] tunnel_common.h" || echo "[MISSING] tunnel_common.h"
	@test -f tunnel_client.c && echo "[OK] tunnel_client.c" || echo "[MISSING] tunnel_client.c"
	@test -f tunnel_server.c && echo "[OK] tunnel_server.c" || echo "[MISSING] tunnel_server.c"
endif
	@echo.
	@echo Compiler: $(CC)
	@echo Flags: $(CFLAGS)
	@echo Libs: $(LDFLAGS)

.PHONY: all client server clean cleanall test-local run-client run-server help check