#!/bin/bash

set -e

INSTALL_DIR="/root/ManLab"
SERVICE_NAME="manlab-init"
SERVICE_UNIT="${SERVICE_NAME}.service"
SERVICE_PATH="/etc/systemd/system/${SERVICE_UNIT}"
MALHASH_DST="$INSTALL_DIR/malware_hashes.txt"

# 0. root 권한 확인
if [ "$EUID" -ne 0 ]; then
  echo "[ERROR] 루트 권한으로 실행해 주세요"
  exit 1
fi

echo "[INFO] Stopping and removing ManLab service..."
sudo systemctl disable "$SERVICE_UNIT" || true
sudo systemctl stop "$SERVICE_UNIT" || true
sudo rm -f "$SERVICE_PATH"
sudo systemctl daemon-reload

echo "[INFO] Killing running ManLab processes..."
sudo pkill -f /root/ManLab/bin/ManLab || true
sudo pkill -f /usr/local/bin/ManLab || true

# 잠깐 기다려서 안전하게 종료되도록 함
sleep 1

echo "[INFO] Removing malware_hashes.txt..."
sudo rm -f "$MALHASH_DST"

echo "[INFO] Removing ManLab files..."
sudo rm -rf "$INSTALL_DIR"
sudo rm -f /usr/local/bin/ManLab

echo "[INFO] Uninstall complete."
