# Makefile
# 1. 바이너리 실행
# 2. 루트 디렉토리 하위에 /ManLab 폴더 생성
# 
# 4. 루트 디렉토리 하위에 /ManLab/db 폴더 생성
#    추후 init를 실행함으로서 db 테이블 생성 및 초기화
# 5. 루트 디렉토리 하위에 /ManLab/rules 폴더 생성
#    git 디렉토리의 /ManLab/rulse 데이터 복사
# 6. 루트 디렉토리 하위에 /ManLab/conf 폴더 생성
#	 git 디렉토리의 /ManLab/conf 데이터 복사
# 7. Manlab에 init를 인자로 전달해 줌으로서 초기화 로직 실행
# 8. Manlab systemd 서비스 유닛 파일을 생성‧설치
#    해당 서비스 파일의 내용은 git 기준 ManLab/.deploy 하위에 존재
#	 PC를 부팅한 후 운영체제가 ManLab 바이너리에 boot_check 를 인자로 전달
# TODO
# ./ManLab이 아닌 ManLab을 입력해도 실행되도록 운영체제 환경 변경

# 컴파일러 설정
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wno-unused-local-typedefs \
           -I./src -I./lib -isystem ./include

# 디렉토리 및 타겟
SRC_DIR = src
LIB_DIR = lib
TARGET = ManLab

INSTALL_DIR = /ManLab
BIN_DIR = $(INSTALL_DIR)/bin
DB_DIR = $(INSTALL_DIR)/db
HASH_DB = $(DB_DIR)/hash.db
QUARANTINE_DB = $(DB_DIR)/quarantine.db
LOG_ANALYSIS_RESULT_DB = $(DB_DIR)/logAnalysisResult.db
CONF_SRC_DIR = conf
CONF_DST_DIR = $(INSTALL_DIR)/conf
RULE_SRC_DIR = rules
RULE_DST_DIR = $(INSTALL_DIR)/rules

SERVICE_NAME   = manlab-init
SERVICE_UNIT   = $(SERVICE_NAME).service
SERVICE_PATH   = /etc/systemd/system/$(SERVICE_UNIT)
SERVICE_TMPL   = ./.deploy/$(SERVICE_UNIT).tmpl

# 소스 파일 직접 나열
SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/CommandHandler.cpp \
       $(SRC_DIR)/DaemonBase.cpp \
       $(SRC_DIR)/DBManager.cpp \
       $(SRC_DIR)/FimCommandHandler.cpp \
       $(SRC_DIR)/SigCommandHandler.cpp \
       $(SRC_DIR)/LogDaemon.cpp \
       $(SRC_DIR)/RealtimeMonitorDaemon.cpp \
       $(SRC_DIR)/ScheduledScanDaemon.cpp \
       $(SRC_DIR)/MalwareScan.cpp \
       $(SRC_DIR)/QuarantineManager.cpp \
	     $(SRC_DIR)/LogStorageManager.cpp \
	     $(SRC_DIR)/baseline_generator.cpp \
       $(LIB_DIR)/INIReader.cpp \
	     $(LIB_DIR)/ini.c

# 빌드 대상
.PHONY: all init install initialize_db copy_conf copy_rules install_service clean 

all: $(TARGET) install initialize_db copy_conf copy_rules install_service
	@$(MAKE) init
	rm -f $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) -lsqlite3 -lyara -lssl -lcrypto -lpthread

install:
	@echo "[INFO] /ManLab/bin 경로를 생성 중..."
	sudo mkdir -p $(BIN_DIR)
	sudo cp $(TARGET) $(BIN_DIR)/$(TARGET)
	sudo ln -sf $(BIN_DIR)/$(TARGET) /usr/local/bin/$(TARGET)

initialize_db:
	@echo "[INFO] /ManLab/db 초기화 중..."
	sudo mkdir -p $(DB_DIR)

copy_conf:
	@echo "[INFO] 설정 파일을 복사합니다..."
	sudo mkdir -p $(CONF_DST_DIR)
	sudo cp -v $(CONF_SRC_DIR)/*.ini $(CONF_DST_DIR)/

copy_rules:
	@echo "[INFO] 룰 파일을 복사합니다..."
	sudo mkdir -p $(RULE_DST_DIR)
	sudo cp -v $(RULE_SRC_DIR)/*.yar $(RULE_DST_DIR)/

init: $(TARGET)
	@echo "[INFO] 데몬 및 데이터 초기화를 진행합니다..."
	sudo ./$(TARGET) init

install_service: $(SERVICE_TMPL)
	@echo "[INFO] systemd 서비스 유닛 설치..."
	sudo sed "s|__MANLAB_BIN__|$(BIN_DIR)/$(TARGET)|" $< | sudo tee $(SERVICE_PATH) > /dev/null
	sudo systemctl daemon-reload
	sudo systemctl enable $(SERVICE_UNIT)
	
clean:
	@echo "[INFO] /ManLab 전체 삭제 중..."
	sudo rm -rf /ManLab

	@echo "[INFO] systemd 서비스 제거 중..."
	sudo systemctl disable $(SERVICE_NAME).service || true
	sudo rm -f /etc/systemd/system/$(SERVICE_NAME).service
	sudo systemctl daemon-reload