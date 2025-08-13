# Makefile for Port Forwarding Module

CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = 

# 偵測作業系統
ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32 -lpthread
    TARGET = portforward.exe
    RM = del /F /Q 2>nul || true
    SEP = \\
else
    CFLAGS += -pthread
    LDFLAGS += -pthread
    TARGET = portforward
    RM = rm -f
    SEP = /
endif

# 源文件
SRCS = port_forward$(SEP)forward_cli.c \
       port_forward$(SEP)port_forward.c \
       core$(SEP)logger.c

# 目標文件
OBJS = forward_cli.o \
       port_forward.o \
       logger.o

# 頭文件目錄
INCLUDES = -I. -Icore -Iport_forward

# 預設目標 - 不需要創建目錄，假設目錄已存在
all: $(TARGET)

# 編譯主程式
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo ================================
	@echo Build successful!
	@echo Run: $(TARGET)
	@echo ================================
	@echo Cleaning up object files...
ifeq ($(OS),Windows_NT)
	@del /F /Q *.o 2>nul || echo Done
else
	@rm -f *.o
endif

# 編譯規則
forward_cli.o: port_forward/forward_cli.c port_forward/port_forward.h
	$(CC) $(CFLAGS) $(INCLUDES) -c port_forward/forward_cli.c -o forward_cli.o

port_forward.o: port_forward/port_forward.c port_forward/port_forward.h
	$(CC) $(CFLAGS) $(INCLUDES) -c port_forward/port_forward.c -o port_forward.o

logger.o: core/logger.c core/logger.h
	$(CC) $(CFLAGS) $(INCLUDES) -c core/logger.c -o logger.o

# 清理 (只清理執行檔和日誌)
clean:
ifeq ($(OS),Windows_NT)
	@del /F /Q $(TARGET) *.log 2>nul || echo Clean complete
else
	@rm -f $(TARGET) *.log
endif

# 深度清理 (包含 .o 檔)
cleanall:
ifeq ($(OS),Windows_NT)
	@del /F /Q *.o $(TARGET) *.log 2>nul || echo Clean complete
else
	@rm -f *.o $(TARGET) *.log
endif

# 執行
run: $(TARGET)
	./$(TARGET)

# 使用配置文件執行
run-config: $(TARGET)
	./$(TARGET) forward.conf

# 創建目錄結構（手動執行）
setup:
ifeq ($(OS),Windows_NT)
	@if not exist port_forward (mkdir port_forward)
	@if not exist core (mkdir core)
	@echo Directory structure created
else
	@mkdir -p port_forward core
	@echo Directory structure created
endif

# 幫助
help:
	@echo Available targets:
	@echo   make        - Build port forwarding tool (auto-clean .o files)
	@echo   make clean  - Clean executable and log files
	@echo   make cleanall - Clean everything including .o files
	@echo   make run    - Build and run
	@echo   make run-config - Run with forward.conf
	@echo   make setup  - Create directory structure
	@echo   make help   - Show this help

.PHONY: all clean cleanall run run-config help setup