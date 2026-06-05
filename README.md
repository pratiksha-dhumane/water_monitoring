# Water Quality Monitoring System

**Arduino + ESP32 + Node.js + HTML Dashboard**

A complete IoT water quality monitor that reads Turbidity, pH, and Temperature sensors,
displays live values on an LCD, sends data to a Node.js backend over WiFi, and
shows a live dashboard with historical charts.

---

## Project Structure

```
water_monitor/
├── arduino/
│   └── arduino_sensor_node.ino    ← Flash to Arduino UNO/Nano
├── esp32/
│   └── esp32_gateway.ino          ← Flash to ESP32
├── backend/
│   ├── package.json
│   ├── server.js                  ← Node.js API server
│   └── data/
│       └── data.txt               ← Auto-created CSV log
└── frontend/
    └── index.html                 ← Dashboard (served by backend)
```

---

## Hardware & Wiring

### Components
| Component              | Qty |
|------------------------|-----|
| Arduino UNO or Nano    | 1   |
| ESP32 dev board        | 1   |
| Turbidity sensor       | 1   |
| pH sensor module       | 1   |
| LM35 or DS18B20        | 1   |
| 16×2 I²C LCD (0x27)   | 1   |
| 1kΩ resistor           | 1   |
| 2kΩ resistor           | 1   |
| Jumper wires           | —   |

### Arduino Pin Connections
| Sensor          | Arduino Pin | Wire type   |
|-----------------|-------------|-------------|
| Turbidity       | A0          | Analog      |
| pH sensor       | A1          | Analog      |
| LM35 temp       | A2          | Analog      |
| DS18B20 (alt.)  | D2          | Digital 1-W |

### Arduino → ESP32 UART (⚠ Voltage divider required!)
| Arduino | — | ESP32   | Note                          |
|---------|---|---------|-------------------------------|
| TX (D1) | → | GPIO16  | Via 1kΩ/2kΩ voltage divider   |
| RX (D0) | ← | GPIO17  | Direct (3.3V safe)            |
| GND     | — | GND     | Must be common ground         |

### Optional Optical LDR Link (no wired RX/TX)
Use `USE_OPTICAL_LINK` in the sketches to transmit sensor data from Arduino to ESP32 using light pulses instead of a UART cable.

| Arduino | → | ESP32 | Note |
|---------|---|-------|------|
| D5 | LED | LDR / GPIO34 | 220Ω LED resistor, 10kΩ pull-down on ADC pin |

Setup:
- Arduino D5 drives a visible LED through a 220Ω resistor.
- ESP32 GPIO34 reads the LDR voltage divider (LDR to 3.3V, 10kΩ to GND).
- Keep LED and LDR aligned and shield from ambient light.
- No data wire is required between boards for the optical path.

**Voltage divider (ESP32 ADC):**
```text
3.3V ── LDR ── GPIO34 ── 10kΩ ── GND
```

**Voltage divider (Arduino LED):**
```text
D5 ── 220Ω ── LED ── GND
```

**Voltage divider (Arduino TX → ESP32 RX):**
```
Arduino TX ──[1kΩ]──┬──[2kΩ]── GND
                    │
               ESP32 RX (GPIO16)
```

### ESP32 → LCD I²C
| ESP32   | LCD pin |
|---------|---------|
| GPIO21  | SDA     |
| GPIO22  | SCL     |
| 3.3V    | VCC     |
| GND     | GND     |

---

## Setup Instructions

### 1. Arduino firmware

1. Open `arduino/arduino_sensor_node.ino` in Arduino IDE.
2. Select board: **Arduino UNO** or **Arduino Nano**.
3. Adjust calibration constants at the top if needed:
   - `PH_VOLTAGE_AT_7`, `PH_VOLTAGE_AT_4` — do a 2-point pH calibration with buffer solutions.
   - If using DS18B20 instead of LM35, uncomment the three DS18B20 lines.
4. Upload.
5. Open Serial Monitor at **9600 baud** — you should see: `T:25.5,PH:7.2,TURB:300`

### 2. ESP32 firmware

1. Install required libraries via Arduino Library Manager:
   - **LiquidCrystal_I2C** (Frank de Brabander)
   - **ArduinoJson** (Benoit Blanchon)
2. Open `esp32/esp32_gateway.ino`.
3. Fill in your credentials:
   ```cpp
   const char* WIFI_SSID     = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   const char* SERVER_URL    = "http://192.168.x.x:3000/sensor-data";
   ```
4. Select board: **ESP32 Dev Module**.
5. Upload. The LCD should show WiFi IP, then live readings.

### 3. Backend server

```bash
cd backend
npm install
node server.js
```

Server starts at **http://localhost:3000**

**API endpoints:**
| Method | Path              | Description                    |
|--------|-------------------|--------------------------------|
| POST   | /sensor-data      | Receive reading from ESP32     |
| GET    | /sensor-data      | Get latest 50 readings (JSON)  |
| GET    | /sensor-latest    | Get single latest reading      |
| GET    | /sensor-export    | Download data CSV              |

### 4. Frontend dashboard

Open **http://localhost:3000** in any browser.
The Node.js backend serves `frontend/index.html` as a static file.

---

## Thresholds & Status Logic

| Parameter   | SAFE        | CAUTION       | UNSAFE        |
|-------------|-------------|---------------|---------------|
| Temperature | 10–35°C     | > 35°C        | (no override) |
| pH          | 6.5–8.5     | 6.0–6.5 / 8.5–9.0 | < 6.0 or > 9.0 |
| Turbidity   | < 100 NTU   | 100–300 NTU   | > 300 NTU     |

---

## Data Format

### Serial (Arduino → ESP32)
```
T:25.5,PH:7.2,TURB:300
```

### API JSON (ESP32 → Backend)
```json
{
  "temperature": 25.5,
  "ph": 7.2,
  "turbidity": 300
}
```

### CSV log (data/data.txt)
```
timestamp,temperature,ph,turbidity,status
2024-01-01T12:01:00.000Z,25.5,7.2,300,SAFE
2024-01-01T12:02:00.000Z,25.7,7.1,310,CAUTION
```

---

## Optional Upgrades

| Feature            | How to add                                          |
|--------------------|-----------------------------------------------------|
| RTC timestamps     | Add DS3231 RTC module to ESP32, replace millis()   |
| SD card logging    | Use ESP32 SD library alongside SPIFFS               |
| Mobile alerts      | Add Twilio SMS or Blynk push notification          |
| MongoDB storage    | Add `mongoose` to backend, swap fs.appendFile       |
| HTTPS              | Add nginx reverse proxy with Let's Encrypt cert     |
| Multi-sensor nodes | Add unique `node_id` field to the JSON POST body   |

---

## Troubleshooting

| Problem                  | Fix                                                        |
|--------------------------|------------------------------------------------------------|
| LCD shows garbage        | Check I²C address — try 0x3F instead of 0x27               |
| ESP32 RX garbled data    | Confirm voltage divider on Arduino TX; check baud match    |
| pH reading always ~7     | Perform 2-point calibration; check sensor module power     |
| Backend not reachable    | Ensure same WiFi network; check firewall on port 3000      |
| SPIFFS mount failed      | Tools → ESP32 Sketch Data Upload; check partition scheme   |
