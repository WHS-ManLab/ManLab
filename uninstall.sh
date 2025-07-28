#!/bin/bash

set -e

INSTALL_DIR="/root/ManLab"
BIN_DIR="$INSTALL_DIR/bin"
CONF_DIR="$INSTALL_DIR/conf"
SERVICE_NAME="manlab-init"
SERVICE_UNIT="${SERVICE_NAME}.service"
SERVICE_PATH="/etc/systemd/system/${SERVICE_UNIT}"
MALHASH_DST="$INSTALL_DIR/malware_hashes.txt"
LINK_PATH="/usr/local/bin/ManLab"

RSYSLOG_CONF="/etc/rsyslog.d/50-manlab.conf"
AUDITD_RULE="/etc/audit/rules.d/manlab.rules"

# 0. root 권한 확인
if [ "$EUID" -ne 0 ]; then
  echo "[ERROR] 루트 권한으로 실행해 주세요"
  exit 1
fi

# 1. 삭제 모드 선택
echo "[PROMPT] 삭제 방식을 선택해 주세요:"
echo "  1. 완전 삭제 (ManLab 전체 삭제)"
echo "  2. 부분 삭제 (데이터 보존)"
read -p "선택 [1/2]: " DELETE_MODE

if [[ "$DELETE_MODE" != "1" && "$DELETE_MODE" != "2" ]]; then
  echo "[ERROR] 잘못된 선택입니다. 1 또는 2를 입력해 주세요."
  exit 1
fi

echo "[INFO] ManLab 서비스 중지 및 해제 중..."
systemctl disable "$SERVICE_UNIT" || true
systemctl stop "$SERVICE_UNIT" || true
rm -f "$SERVICE_PATH"
systemctl daemon-reload

echo "[INFO] 실행 중인 ManLab 프로세스 종료 중..."
pkill -f "$BIN_DIR/ManLab" || true
pkill -f "$LINK_PATH" || true
sleep 1

echo "[INFO] 설정파일 및 바이너리 제거 중..."
rm -f "$MALHASH_DST"
rm -rf "$CONF_DIR"
rm -rf "$BIN_DIR"
rm -f "$LINK_PATH"

echo "[INFO] rsyslog 및 auditd 설정 제거 중..."
rm -f "$RSYSLOG_CONF"
rm -f "$AUDITD_RULE"
systemctl restart rsyslog || true
augenrules --load > /dev/null 2>&1 || true

if [[ "$DELETE_MODE" == "1" ]]; then
  echo "[INFO] 완전 삭제를 수행합니다. 모든 데이터가 삭제됩니다."
  rm -rf "$INSTALL_DIR"
else
  echo "[INFO] 부분 삭제를 수행합니다. DB 및 보고서 등은 남겨둡니다."
fi

echo "[OK] ManLab 삭제가 완료되었습니다."