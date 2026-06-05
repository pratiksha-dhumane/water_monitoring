// ============================================================
//  Water Quality Monitor — Node.js / Express Backend
//
//  Endpoints:
//    POST /sensor-data   — receive reading from ESP32
//    GET  /sensor-data   — return last N readings (JSON)
//    GET  /sensor-latest — return single latest reading
//    GET  /sensor-export — download data.csv
//
//  Storage:
//    • In-memory ring buffer (last MAX_MEMORY readings)
//    • data/data.txt — append-only CSV on disk
//
//  Start:
//    npm install
//    node server.js
// ============================================================

const express  = require('express');
const cors     = require('cors');
const fs       = require('fs');
const path     = require('path');

const app  = express();
const PORT = process.env.PORT || 3000;

// ── Middleware ────────────────────────────────────────────────
app.use(cors());
app.use(express.json());
app.use(express.static(__dirname));  // serve dashboard from current directory

// ── Storage setup ─────────────────────────────────────────────
const DATA_DIR  = path.join(__dirname, 'data');
const DATA_FILE = path.join(DATA_DIR, 'data.txt');
const MAX_MEMORY = 500;   // keep last 500 readings in RAM

if (!fs.existsSync(DATA_DIR)) fs.mkdirSync(DATA_DIR, { recursive: true });

// Ensure CSV header exists
if (!fs.existsSync(DATA_FILE)) {
  fs.writeFileSync(DATA_FILE, 'timestamp,temperature,ph,turbidity,status\n');
}

// In-memory ring buffer
let readings = [];

// ── Helpers ───────────────────────────────────────────────────

function determineStatus(temp, ph, turbidity) {
  const phOk   = ph >= 6.5 && ph <= 8.5;
  const turbOk = turbidity < 100;
  const tempOk = temp >= 10  && temp <= 35;

  if (ph < 6.0 || ph > 9.0 || turbidity > 300) return 'UNSAFE';
  if (!phOk || !turbOk || !tempOk)              return 'CAUTION';
  return 'SAFE';
}

function appendToFile(record) {
  const line = `${record.timestamp},${record.temperature},${record.ph},${record.turbidity},${record.status}\n`;
  fs.appendFile(DATA_FILE, line, (err) => {
    if (err) console.error('[File] Write error:', err.message);
  });
}

function addReading(record) {
  readings.push(record);
  if (readings.length > MAX_MEMORY) readings.shift();   // drop oldest
  appendToFile(record);
}

// ── Routes ────────────────────────────────────────────────────

/**
 * POST /sensor-data
 * Body: { "temperature": 25.5, "ph": 7.2, "turbidity": 300 }
 * Called by ESP32 every ~10 seconds.
 */
app.post('/sensor-data', (req, res) => {
  const { temperature, ph, turbidity, status: clientStatus } = req.body;

  // Validate
  if (temperature === undefined || ph === undefined || turbidity === undefined) {
    return res.status(400).json({ error: 'Missing fields: temperature, ph, turbidity required' });
  }

  const temp  = parseFloat(temperature);
  const phVal = parseFloat(ph);
  const turb  = parseFloat(turbidity);

  if (isNaN(temp) || isNaN(phVal) || isNaN(turb)) {
    return res.status(400).json({ error: 'Non-numeric sensor values' });
  }

  const record = {
    id:          Date.now(),
    timestamp:   new Date().toISOString(),
    temperature: parseFloat(temp.toFixed(2)),
    ph:          parseFloat(phVal.toFixed(2)),
    turbidity:   parseFloat(turb.toFixed(1)),
    status:      clientStatus || determineStatus(temp, phVal, turb),
  };

  addReading(record);

  console.log(`[POST] ${record.timestamp}  T=${record.temperature}  pH=${record.ph}  Turb=${record.turbidity}  → ${record.status}`);
  res.status(201).json({ success: true, record });
});

/**
 * GET /sensor-data?limit=50
 * Returns latest N readings (default 50, max 500).
 */
app.get('/sensor-data', (req, res) => {
  const limit = Math.min(parseInt(req.query.limit) || 50, MAX_MEMORY);
  const result = readings.slice(-limit);
  res.json({
    count: result.length,
    latest: result[result.length - 1] || null,
    readings: result,
  });
});

/**
 * GET /sensor-latest
 * Returns only the most recent reading.
 */
app.get('/sensor-latest', (req, res) => {
  if (readings.length === 0) return res.status(404).json({ error: 'No data yet' });
  res.json(readings[readings.length - 1]);
});

/**
 * GET /sensor-export
 * Downloads the full data.txt CSV file.
 */
app.get('/sensor-export', (req, res) => {
  if (!fs.existsSync(DATA_FILE)) return res.status(404).send('No data file');
  res.download(DATA_FILE, 'water_quality_data.csv');
});

/**
 * DELETE /sensor-data
 * Clears in-memory buffer (does not touch the file).
 */
app.delete('/sensor-data', (req, res) => {
  readings = [];
  console.log('[DELETE] In-memory buffer cleared');
  res.json({ success: true, message: 'Buffer cleared' });
});

// ── Start server ──────────────────────────────────────────────
app.listen(PORT, () => {
  console.log(`\n╔══════════════════════════════════════╗`);
  console.log(`║  Water Monitor Backend running on    ║`);
  console.log(`║  http://localhost:${PORT}               ║`);
  console.log(`╠══════════════════════════════════════╣`);
  console.log(`║  POST /sensor-data   — ESP32 input   ║`);
  console.log(`║  GET  /sensor-data   — latest N      ║`);
  console.log(`║  GET  /sensor-latest — single record ║`);
  console.log(`║  GET  /sensor-export — CSV download  ║`);
  console.log(`╚══════════════════════════════════════╝\n`);
});
