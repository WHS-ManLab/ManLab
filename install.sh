#!/bin/bash

set -e

INSTALL_DIR="/root/ManLab"
BIN_DIR="$INSTALL_DIR/bin"
DB_DIR="$INSTALL_DIR/db"
CONF_SRC_DIR="conf"
RULE_SRC_DIR="rules"
CONF_DST_DIR="$INSTALL_DIR/conf"
RULE_DST_DIR="$INSTALL_DIR/rules"
QUARANTINE_DIR="$INSTALL_DIR/quarantine"
MALWARE_DIR="$INSTALL_DIR/malware"
REPORT_DIR="$INSTALL_DIR/report"

SERVICE_NAME="manlab-init"
SERVICE_UNIT="${SERVICE_NAME}.service"
SERVICE_PATH="/etc/systemd/system/${SERVICE_UNIT}"
SERVICE_TMPL="./.deploy/${SERVICE_UNIT}"  
RSYSLOG_CONF="/etc/rsyslog.d/50-manlab.conf"
RSYSLOG_SRC="./.deploy/50-manlab.conf"

MALHASH_SRC="malhash/malware_hashes.txt"
MALHASH_DST="$MALWARE_DIR/malware_hashes.txt"



# 0. root 권한 확인
if [ "$EUID" -ne 0 ]; then
  echo "[ERROR] 루트 권한으로 실행해 주세요"
  exit 1
fi

echo "[INFO] Installing ManLab..."

# 1. 디렉토리 생성 및 바이너리 설치
mkdir -p "$BIN_DIR"
cp -v ManLab "$BIN_DIR/ManLab"
cp -v ManLab-cli "$BIN_DIR/ManLab-cli"
ln -sf "$BIN_DIR/ManLab-cli" /usr/local/bin/ManLab

# 2. DB 및 기타 디렉토리
mkdir -p "$DB_DIR" "$RULE_DST_DIR" "$CONF_DST_DIR" "$QUARANTINE_DIR" "$MALWARE_DIR" "$REPORT_DIR"
chown "$USER":"$USER" "$QUARANTINE_DIR"

# 3. 설정 및 룰 복사
cp -v "$CONF_SRC_DIR"/*.ini "$CONF_DST_DIR"/ || true
cp -v "$CONF_SRC_DIR"/*.yaml "$CONF_DST_DIR"/ || true
cp -v "$RULE_SRC_DIR"/*.yar "$RULE_DST_DIR"/ || true

# 4. rsyslog 설정
echo "[INFO] Setting up rsyslog..."
apt install -y rsyslog
cp -v "$RSYSLOG_SRC" "$RSYSLOG_CONF"
systemctl restart rsyslog

# 5. systemd 서비스 설치
echo "[INFO] Installing systemd service..."
sed "s|__MANLAB_BIN__|$BIN_DIR/ManLab|" "$SERVICE_TMPL" > "$SERVICE_PATH"
chmod 644 "$SERVICE_PATH"
systemctl daemon-reload
systemctl enable "$SERVICE_UNIT"

# 6. malware_hashes.txt 복사
echo "[INFO] Copying malware_hashes.txt..."
cp -v "$MALHASH_SRC" "$MALHASH_DST"

# 7. auditd 설치
echo "[INFO] Installing auditd..."
apt install -y auditd
systemctl enable auditd
systemctl start auditd

# 8. spdlog 설치
echo "[INFO] Installing spdlog..."
sudo apt install libspdlog-dev

echo "[OK] ManLab installation completed."
systemctl start "$SERVICE_UNIT"