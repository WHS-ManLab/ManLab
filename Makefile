# Makefile - Build only

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wno-unused-local-typedefs \
           -I. -I./src -I./lib -I./utils -isystem ./include

SRC_DIR = src
LIB_DIR = lib
UTILS_DIR = utils
TARGET = ManLab

SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/CommandHandler.cpp \
       $(SRC_DIR)/DBManager.cpp \
       $(SRC_DIR)/FimCommandHandler.cpp \
       $(SRC_DIR)/SigCommandHandler.cpp \
       $(SRC_DIR)/RealtimeMonitorDaemon.cpp \
	   $(SRC_DIR)/RealTimeMonitor.cpp \
       $(SRC_DIR)/ScheduledScanWatcher.cpp \
	   $(SRC_DIR)/ScheduledScanExecutor.cpp \
       $(SRC_DIR)/MalwareScan.cpp \
       $(SRC_DIR)/QuarantineManager.cpp \
	   $(SRC_DIR)/LogStorageManager.cpp \
	   $(SRC_DIR)/RestoreManager.cpp \
	   $(SRC_DIR)/DaemonUtils.cpp \
	   $(SRC_DIR)/RsyslogManager.cpp \
	   $(SRC_DIR)/RsyslogRule.cpp \
	   $(SRC_DIR)/ScheduledScan.cpp \
	   $(SRC_DIR)/FIMBaselineGenerator.cpp \
	   $(SRC_DIR)/ServerDaemon.cpp \
	   $(SRC_DIR)/CommandExecutor.cpp \
	   $(SRC_DIR)/CommandReceiver.cpp \
	   $(SRC_DIR)/FIMIntegScan.cpp \
	   $(SRC_DIR)/UserNotifier.cpp \
	   $(SRC_DIR)/GmailClient.cpp \
	   $(SRC_DIR)/ReportService.cpp \
	   $(UTILS_DIR)/StringUtils.cpp \
       $(LIB_DIR)/INIReader.cpp \
	   $(LIB_DIR)/ini.c

LDLIBS = -lyaml-cpp -lsqlite3 -lyara -lssl -lcrypto -lpthread -lsystemd -lcurl

.PHONY: all clean

all:
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDLIBS)
	@echo "[OK] Build completed "

clean:
	rm -f $(TARGET)
	@echo "[INFO] Binary cleaned."
