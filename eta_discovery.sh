#!/usr/bin/env bash
set -euo pipefail

# ====== ⚠️ ANPASSEN ======
BROKER="MQTT_SERVER_IP"
PORT="1883"
USER="MQTT_USER"
PASS="MQTT_PASSWORD"
# =========================

DISC_PREFIX="homeassistant"
DEVICE_ID="eta_sh20"
DEVICE_NAME="ETA SH20"
BASE_TOPIC="heizung/status"

pub() { mosquitto_pub -h "$BROKER" -p "$PORT" -u "$USER" -P "$PASS" -t "$1" -m "$2" -r; }

sensor(){
  key="$1"; name="$2"; unit="$3"; devclass="$4"
  topic="$DISC_PREFIX/sensor/$DEVICE_ID/$key/config"
  payload=$(cat <<EOF
{"name":"$name","uniq_id":"${DEVICE_ID}_${key}",
"stat_t":"${BASE_TOPIC}/${key}","unit_of_meas":"$unit","dev_cla":$devclass,
"avty_t":"${BASE_TOPIC}/availability","pl_avail":"online","pl_not_avail":"offline",
"dev":{"ids":["$DEVICE_ID"],"name":"$DEVICE_NAME","mf":"ETA","mdl":"SH20"}}
EOF
)
  pub "$topic" "$payload"
}

echo "Sende Discovery an $BROKER:$PORT ..."
sensor puffer_oben "Puffer Oben" "°C" "\"temperature\""
sensor puffer_mitte "Puffer Mitte" "°C" "\"temperature\""
sensor puffer_unten "Puffer Unten" "°C" "\"temperature\""
sensor boiler "Boiler" "°C" "\"temperature\""
sensor aussen "Außen" "°C" "\"temperature\""
sensor puffer_ladezustand "Puffer Ladezustand" "%" "null"
sensor abgas "Abgas" "°C" "\"temperature\""
sensor kessel "Kessel" "°C" "\"temperature\""
sensor ruecklauf "Rücklauf" "°C" "\"temperature\""
sensor vorlauf_mk1 "Vorlauf MK1" "°C" "\"temperature\""
echo "Fertig."
