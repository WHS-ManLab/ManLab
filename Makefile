CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wno-unused-local-typedefs \
           -I./src/server -I./src/cli -I./src/shared -I./lib -I./utils -isystem ./include

# 디렉토리 경로
SERVER_DIR = src/server
CLI_DIR = src/cli
SHARED_DIR = src/shared
LIB_DIR = lib
UTILS_DIR = utils

# 타겟 이름
SERVER_TARGET = ManLab
CLI_TARGET = ManLab-cli

# 서버용 소스 목록
SERVER_SRCS = \
    $(SERVER_DIR)/main.cpp \
    $(SERVER_DIR)/ServerDaemon.cpp \
    $(SERVER_DIR)/DaemonUtils.cpp \
    $(SERVER_DIR)/CommandExecutor.cpp \
    $(SERVER_DIR)/CommandReceiver.cpp \
    $(SERVER_DIR)/RealtimeMonitorDaemon.cpp \
    $(SERVER_DIR)/RealTimeMonitor.cpp \
    $(SERVER_DIR)/ScheduledScanWatcher.cpp \
    $(SERVER_DIR)/ScheduledScanExecutor.cpp \
    $(SERVER_DIR)/MalwareScan.cpp \
    $(SERVER_DIR)/QuarantineManager.cpp \
    $(SERVER_DIR)/LogStorageManager.cpp \
    $(SERVER_DIR)/RestoreManager.cpp \
    $(SERVER_DIR)/RsyslogManager.cpp \
    $(SERVER_DIR)/RsyslogRule.cpp \
    $(SERVER_DIR)/ScheduledScan.cpp \
    $(SERVER_DIR)/FIMBaselineGenerator.cpp \
    $(SERVER_DIR)/FIMIntegScan.cpp \
    $(SERVER_DIR)/UserNotifier.cpp \
    $(SERVER_DIR)/FimCommandHandler.cpp \
    $(SERVER_DIR)/SigCommandHandler.cpp \
    $(SERVER_DIR)/DBManager.cpp \
    $(SERVER_DIR)/ReportService.cpp \
    $(SERVER_DIR)/GmailClient.cpp \

# 클라이언트용 소스 목록
CLI_SRCS = \
    $(CLI_DIR)/main.cpp \
    $(CLI_DIR)/CommandHandler.cpp

# 공통 유틸 / 외부 라이브러리 소스
UTIL_SRCS = $(UTILS_DIR)/StringUtils.cpp
LIB_SRCS = $(LIB_DIR)/INIReader.cpp $(LIB_DIR)/ini.c

# 링커 라이브러리
LDLIBS = -lyaml-cpp -lsqlite3 -lyara -lssl -lcrypto -lpthread -lsystemd -lcurl

.PHONY: all clean

all: $(SERVER_TARGET) $(CLI_TARGET)
	@echo "[OK] Build completed"

$(SERVER_TARGET): $(SERVER_SRCS) $(UTIL_SRCS) $(LIB_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(CLI_TARGET): $(CLI_SRCS) $(UTIL_SRCS) $(LIB_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(SERVER_TARGET) $(CLI_TARGET)
	@echo "[INFO] Binaries cleaned"