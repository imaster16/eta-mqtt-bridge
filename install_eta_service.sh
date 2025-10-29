#!/usr/bin/env bash
set -euo pipefail

# ====== ⚠️ ANPASSEN (DUMMY-WERTE ERSETZEN) ======
MQTT_HOST="MQTT_SERVER_IP"     # z.B. 192.168.1.10
MQTT_PORT="1883"
MQTT_USER="MQTT_USER"
MQTT_PASS="MQTT_PASSWORD"
TTY_DEV="/dev/ttyUSB0"
TOPIC="heizung/status"
INTERVAL="30"
ABGAS_FACTOR="0.5373"
# ===============================================

BIN_SRC="${BIN_SRC:-./src/eta_v3.9.2.c}"
BIN_OUT="eta_v3.9.2"
INSTALL_DIR="/opt/eta"
SERVICE_NAME="eta"
ETA_USER="eta"

# Build dependencies
sudo apt update
sudo apt install -y build-essential libmosquitto-dev

# Build
gcc -O2 -Wall -pthread -o "$BIN_OUT" "$BIN_SRC" -lmosquitto

# System user & install
id -u "$ETA_USER" &>/dev/null || sudo useradd -r -s /usr/sbin/nologin "$ETA_USER"
sudo usermod -aG dialout "$ETA_USER" || true

sudo mkdir -p "$INSTALL_DIR"
sudo cp "./$BIN_OUT" "$INSTALL_DIR/"
sudo chown -R "$ETA_USER:$ETA_USER" "$INSTALL_DIR"
sudo chmod 755 "$INSTALL_DIR/$BIN_OUT"

# Env file
sudo tee /etc/eta-mqtt.env >/dev/null <<EOF
MQTT_HOST=$MQTT_HOST
MQTT_PORT=$MQTT_PORT
MQTT_USER=$MQTT_USER
MQTT_PASS=$MQTT_PASS
TTY_DEV=$TTY_DEV
TOPIC=$TOPIC
INTERVAL=$INTERVAL
ABGAS_FACTOR=$ABGAS_FACTOR
EOF
sudo chmod 600 /etc/eta-mqtt.env

# systemd unit
sudo tee /etc/systemd/system/${SERVICE_NAME}.service >/dev/null <<'EOF'
[Unit]
Description=ETA Heizungs-MQTT-Bridge
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
User=eta
Group=eta
WorkingDirectory=/opt/eta
EnvironmentFile=/etc/eta-mqtt.env
ExecStart=/opt/eta/eta_v3.9.2 ${TTY_DEV}
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now ${SERVICE_NAME}.service

echo "Installation fertig. Status:"
systemctl status ${SERVICE_NAME}.service --no-pager
