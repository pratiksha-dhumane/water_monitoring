# 🔌 Arduino ↔ ESP32 Connection Guide

## ⚠️ CRITICAL: Voltage Divider (5V → 3.3V)

**Arduino outputs 5V, ESP32 GPIO accepts 3.3V MAX**

### Voltage Divider Circuit
```
Arduino TX (5V)
    |
    +--[1kΩ]--+---> ESP32 GPIO16 (RXD2)
    |         |
   GND      [2kΩ]
            |
           GND
```

**Calculation:** 5V × (2kΩ / (1kΩ + 2kΩ)) = 3.33V ✓

### What You Need
- 1x 1kΩ resistor
- 1x 2kΩ resistor  
- Jumper wires
- USB cables (for uploading & monitoring)

---

## 🔗 Physical Connections

| Arduino | → | ESP32 |
|---------|---|-------|
| TX      | → | GPIO16 (RXD2) + Voltage Divider |
| GND     | → | GND   |

**That's it!** No other pins needed for basic serial communication.

## 🌞 Optional Optical LDR Link
If you want a no-wire data path, use an LED on the Arduino and an LDR on the ESP32.

| Arduino | → | ESP32 |
|---------|---|-------|
| D5 | LED | LDR / GPIO34 |

### Optical wiring
- Arduino D5 → 220Ω resistor → LED → GND
- ESP32 GPIO34 → LDR → 3.3V
- ESP32 GPIO34 → 10kΩ → GND

Keep the LED and LDR aligned and shield them from bright ambient light. The optical link does not require a data wire between boards.

---

## 📝 Step-by-Step Setup

### 1️⃣ **Upload Arduino Code**
- Open `arduino_sensor_node_test.ino` in Arduino IDE
- Select Board: **Arduino Uno** (or your model)
- Upload to Arduino

### 2️⃣ **Upload ESP32 Code**
- Open `esp32_gateway_test.ino` in Arduino IDE
- Select Board: **ESP32 Dev Module** (or your model)
- Upload to ESP32

### 3️⃣ **Monitor Serial Output**
- Open **Tools → Serial Monitor** in Arduino IDE
- Set baud rate: **115200**
- You should see:
```
================================================
  ESP32 Gateway — Test Mode
================================================
Waiting for data from Arduino...

Received: T:25.5,PH:7.2,TURB:300
------ PARSED DATA ------
Temperature: 25.5 °C
pH: 7.2
Turbidity: 300 NTU
-------------------------
```

---

## ❌ Troubleshooting

| Problem | Solution |
|---------|----------|
| **Nothing showing in Serial Monitor** | Check baud rate (ESP32=115200, Arduino=9600) |
| **Garbage characters** | Verify baud rates match, check USB cable |
| **"Format Error"** | Arduino not sending newline (`\n`) at end of data |
| **No data received** | Check voltage divider, verify GPIO16 connection |
| **ESP32 won't upload** | Install **ESP32 Board Manager** in Arduino IDE |

---

## 📊 Data Format

Arduino must send data in this exact format:
```
T:25.5,PH:7.2,TURB:300\n
```

- `T:` = Temperature (float)
- `PH:` = pH value (float)
- `TURB:` = Turbidity (integer)
- **MUST end with newline (`\n`)**

---

## 🎯 Next Steps (Production Code)

Once testing works:
1. Replace with `arduino_sensor_node.ino` (reads real sensors)
2. Use `esp32_gateway.ino` (adds WiFi, LCD, data logging)
3. Connect WiFi & backend server
4. Deploy to live water monitoring system

---

**Happy testing!** 🚀
