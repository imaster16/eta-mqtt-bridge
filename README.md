# eta-mqtt-bridge

ETA SH (RS232) → MQTT Bridge mit Einzel-Topics, RAW-Werten und Availability (online/offline).

## Features
- Liest ETA-Werte (10 Kanäle) über RS232 (19200 8N1)
- MQTT Publish je Kanal: `heizung/status/<key>` + `<key>_raw`
- Availability: `heizung/status/availability` = `online`/`offline`
- Faktor-Änderung zur Laufzeit via MQTT:  
  `heizung/cmd` → Payload: `set_factor abgas=0.5373`
- Home Assistant Unterstützung (MQTT Discovery Script)

## Build
```bash
sudo apt update
sudo apt install -y build-essential libmosquitto-dev
gcc -O2 -Wall -pthread -o eta_v3.9.2 src/eta_v3.9.2.c -lmosquitto
Run (manuell)
./eta_v3.9.2 /dev/ttyUSB0


⚠️ ANPASSEN IM CODE:

MQTT_SERVER_IP → IP deines Brokers

MQTT_USER / MQTT_PASSWORD

Installation als Dienst
chmod +x scripts/install_eta_service.sh
sudo scripts/install_eta_service.sh


⚠️ ANPASSEN IM SCRIPT install_eta_service.sh:

MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS

TTY_DEV (z. B. /dev/ttyUSB0)

Home Assistant (MQTT Discovery)
chmod +x scripts/eta_discovery.sh
# ⚠️ ANPASSEN: BROKER/PORT/USER/PASS im Script
scripts/eta_discovery.sh

Topics

Werte: heizung/status/<key> (float, retained)

RAW: heizung/status/<key>_raw (int, retained)

Availability: heizung/status/availability (online/offline, retained)

Command: heizung/cmd → set_factor <key>=<wert>

Uninstall
chmod +x scripts/uninstall_eta_service.sh
sudo scripts/uninstall_eta_service.sh

Lizenz

MIT – siehe LICENSE.


---

##  Lokal initialisieren & nach GitHub pushen

### Variante A: Standard `git` (über HTTPS)
```bash
# in den Projektordner
cd eta-mqtt-bridge

git init
git add .
git commit -m "Initial commit: eta-mqtt-bridge v3.9.2"

# ⚠️ Deine Repo-URL einsetzen:
git branch -M main
git remote add origin https://github.com/DEIN_GITHUB_USERNAME/eta-mqtt-bridge.git   # ⚠️ ANPASSEN
git push -u origin main

Variante B: GitHub CLI (gh)
# installieren, falls nicht vorhanden:
# sudo apt install gh -y
# gh auth login

cd eta-mqtt-bridge
git init && git add . && git commit -m "Initial commit"
gh repo create eta-mqtt-bridge --public --source=. --remote=origin --push


Danach siehst du das Repo online in deinem GitHub-Account.

3) Verbindung zu Home Assistant über MQTT – Kurzguide

In Home Assistant: Einstellungen → Geräte & Dienste → MQTT

Integration hinzufügen (falls noch nicht)

Broker-IP/Port notieren

Discovery senden (einmalig):


# ⚠️ ANPASSEN in scripts/eta_discovery.sh (BROKER/PORT/USER/PASS)
scripts/eta_discovery.sh


→ Gerät „ETA SH20“ erscheint in HA, Sensoren werden automatisch angelegt.

Werte prüfen:

mosquitto_sub -h MQTT_SERVER_IP -p 1883 -u MQTT_USER -P MQTT_PASSWORD -t 'heizung/status/#' -v   # ⚠️ ANPASSEN

4) Wo musst du anpassen? (alle MARKER)

src/eta_v3.9.2.c

MQTT_SERVER_IP → ⚠️ (deine Broker-IP)

MQTT_USER → ⚠️

MQTT_PASSWORD → ⚠️

scripts/install_eta_service.sh

MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS → ⚠️

TTY_DEV → ⚠️ (z. B. /dev/ttyUSB0)

scripts/eta_discovery.sh

BROKER, PORT, USER, PASS → ⚠️

Git Push

GitHub-URL mit deinem Benutzernamen → ⚠️

Wenn du willst, packe ich dir noch eine udev-Regel für einen stabilen Gerätesymlink (/dev/eta-sh20) oder
