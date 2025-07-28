# 컴파일러 및 옵션
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wno-unused-local-typedefs \
           -I./src/server -I./src/cli -I./src/shared -I./lib -I./utils -isystem ./include

LDLIBS = -lyaml-cpp -lsqlite3 -lyara -lssl -lcrypto -lpthread -lsystemd -lcurl

# 디렉토리
SERVER_DIR = src/server
CLI_DIR = src/cli
SHARED_DIR = src/shared
LIB_DIR = lib
UTILS_DIR = utils
BUILD_DIR = build

# 타겟
SERVER_TARGET = ManLab
CLI_TARGET = ManLab-cli

# 서버 소스 목록
SERVER_SRCS = \
    $(SERVER_DIR)/main.cpp \
    $(SERVER_DIR)/ServerDaemon.cpp \
    $(SERVER_DIR)/DaemonUtils.cpp \
    $(SERVER_DIR)/CommandReceiver.cpp \
    $(SERVER_DIR)/CommandBus.cpp \
    $(SERVER_DIR)/RegisterCommands.cpp \
    $(SERVER_DIR)/RealtimeMonitorDaemon.cpp \
    $(SERVER_DIR)/RealTimeMonitor.cpp \
    $(SERVER_DIR)/ScheduleWatcher.cpp \
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
    $(SERVER_DIR)/ScheduledReportExecutor.cpp \
    $(SERVER_DIR)/RealTimeScanWorker.cpp \
    $(SERVER_DIR)/ScanQueue.cpp \
    $(SERVER_DIR)/ScanWatchThread.cpp \
    $(SERVER_DIR)/AuditLogManager.cpp \
    $(SERVER_DIR)/FimLogToDB.cpp \
    $(SERVER_DIR)/CommandHelp.cpp

# 클라이언트 소스
CLI_SRCS = \
    $(CLI_DIR)/main.cpp \
    $(CLI_DIR)/CommandHandler.cpp

# 유틸/라이브러리 소스
UTIL_SRCS = \
     $(UTILS_DIR)/StringUtils.cpp \
     $(UTILS_DIR)/ScheduleParser.cpp

LIB_SRCS = $(LIB_DIR)/INIReader.cpp $(LIB_DIR)/ini.c

# 오브젝트 파일 변환
SERVER_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SERVER_SRCS))
CLI_OBJS    = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(CLI_SRCS))
UTIL_OBJS   = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(UTIL_SRCS))
LIB_OBJS_CPP = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(filter %.cpp, $(LIB_SRCS)))
LIB_OBJS_C   = $(patsubst %.c, $(BUILD_DIR)/%.o, $(filter %.c, $(LIB_SRCS)))

ALL_SERVER_OBJS = $(SERVER_OBJS) $(UTIL_OBJS) $(LIB_OBJS_CPP) $(LIB_OBJS_C)
ALL_CLI_OBJS    = $(CLI_OBJS) $(UTIL_OBJS) $(LIB_OBJS_CPP) $(LIB_OBJS_C)

.PHONY: all clean

# 전체 빌드
all: $(SERVER_TARGET) $(CLI_TARGET)
	@echo "[OK] Build completed"

# 서버 타겟 빌드
$(SERVER_TARGET): $(ALL_SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

# 클라이언트 타겟 빌드
$(CLI_TARGET): $(ALL_CLI_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

# cpp → o
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# c → o
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -std=c99 -Wall -I./lib -c $< -o $@

# 정리
clean:
	rm -rf $(SERVER_TARGET) $(CLI_TARGET) $(BUILD_DIR)
	@echo "[INFO] Binaries cleaned"
