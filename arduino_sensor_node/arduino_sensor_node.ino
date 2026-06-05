// ============================================================
//  Water Quality Monitor — Arduino Sensor Node (UNDERWATER TRANSMITTER)
//  Reads: pH (A0), Turbidity (A1), Temperature (D2/DS18B20)
//  Laser Indicator: D7 (always ON)
//  Outputs: "T:25.5,PH:7.2,TURB:300\n" via NRF24L01 every 2s
// ============================================================

#include <SPI.h>
#include <RF24.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ── Pin definitions ─────────────────────────────────────────
#define PH_PIN         A0     // pH Sensor analog output
#define TURBIDITY_PIN  A1     // Turbidity Sensor analog output
#define TEMP_PIN       2      // DS18B20 Temperature Sensor (1-Wire) on D2
#define LASER_PIN      7      // Laser Module signal pin on D7

// ── NRF24L01+ wireless link ─────────────────────────────────────────
#define NRF_CE_PIN     9     // Arduino digital pin for CE (Chip Enable)
#define NRF_CSN_PIN    10    // Arduino digital pin for CSN (Chip Select Not)
RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);
const byte addr[6] = "00001";  // 5-byte address for pipe 0
#define NRF_PAYLOAD_SIZE 32
char nrf_payload[NRF_PAYLOAD_SIZE];


// ── Calibration constants ────────────────────────────────────
// pH sensor: adjust these two points after calibration with buffer solutions
#define PH_VOLTAGE_AT_7   2.90   // volts at pH 7 (midpoint buffer)
#define PH_VOLTAGE_AT_4   3.40   // volts at pH 4 (acid buffer)

// ADC Reference: 5V, 10-bit resolution (1023 steps)
#define ADC_REF_VOLTAGE   5.0
#define ADC_RESOLUTION    1023.0

// Turbidity: raw ADC 0–1023 mapped to NTU 0–1000 (invert: high voltage = clear)
#define TURB_NTU_MAX      300.0

// ── Sample averaging ─────────────────────────────────────────
#define SAMPLES           20     // readings averaged per sensor per cycle

// ── Timing ───────────────────────────────────────────────────
#define SEND_INTERVAL_MS  2000

// ── DS18B20 Temperature Sensor (1-Wire) ─────────────────────────────
OneWire oneWire(TEMP_PIN);
DallasTemperature ds18b20Sensor(&oneWire);

// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);        // DEBUG only
  
  // Initialize Laser indicator
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, HIGH);  // Laser always ON
  Serial.println("[LASER] Initialized as indicator (always ON)");
  
  // Initialize DS18B20 Temperature Sensor
  ds18b20Sensor.begin();
  Serial.println("[DS18B20] Initialized");
  
  // Initialize NRF24L01+
  if (!radio.begin()) {
    Serial.println("[NRF] Init failed!");
    while (1) { delay(1000); }  // Halt
  }
  radio.setPALevel(RF24_PA_LOW);      // Low power for close range
  radio.setDataRate(RF24_250KBPS);    // Slow but reliable
  radio.openWritingPipe(addr);
  radio.stopListening();              // Arduino is TX only
  
  Serial.println("[NRF] Initialized OK");
  delay(500);
}

void loop() {
  float temperature = readTemperature();
  float phValue     = readPH();
  float turbidity   = readTurbidity();

  String payload = "T:";
  payload += String(temperature, 1);
  payload += ",PH:";
  payload += String(phValue, 1);
  payload += ",TURB:";
  payload += String((int)turbidity);
  payload += "\n";

  // Send via NRF24L01
  payload.toCharArray(nrf_payload, NRF_PAYLOAD_SIZE);
  if (radio.write(&nrf_payload, NRF_PAYLOAD_SIZE)) {
    Serial.print("[TX] ");
    Serial.println(payload);
  } else {
    Serial.println("[TX] Failed!");
  }

  delay(SEND_INTERVAL_MS);
}

// ─── Sensor reading functions ─────────────────────────────────

float readTemperature() {
  // ── DS18B20 Temperature Sensor ──────────────────────────────
  ds18b20Sensor.requestTemperatures();
  float tempC = ds18b20Sensor.getTempCByIndex(0);
  return tempC;
}

float readPH() {
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(PH_PIN);
    delay(20);
  }
  float avgADC  = (float)sum / SAMPLES;
  float voltage = (avgADC / ADC_RESOLUTION) * ADC_REF_VOLTAGE;

  // Two-point linear calibration: slope from pH 4 and pH 7 buffer points
  float slope   = (7.0 - 4.0) / (PH_VOLTAGE_AT_7 - PH_VOLTAGE_AT_4);
  float phValue = 7.0 + slope * (PH_VOLTAGE_AT_7 - voltage);

  phValue = constrain(phValue, 0.0, 14.0);
  return phValue;
}

float readTurbidity() {
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(TURBIDITY_PIN);
    delay(20);
  }
  float avgADC = (float)sum / SAMPLES;

  // Turbidity sensors output HIGH voltage for CLEAR water.
  // Invert: NTU rises as voltage falls.
  float ntu = map((long)avgADC, 0, 1023, (long)TURB_NTU_MAX, 0);
  ntu = constrain(ntu, 0, TURB_NTU_MAX);
  return ntu;
}
