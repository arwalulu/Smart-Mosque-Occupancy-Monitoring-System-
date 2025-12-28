/*
  Smart Mosque Occupancy Monitoring System
  ---------------------------------------
  - Two IR sensors:
      Entry sensor  -> increments occupancy
      Exit sensor   -> decrements occupancy
  - Optional relay:
      Trigger when occupancy reaches CAPACITY_LIMIT (or other rule)

  Wiring (default):
    Entry IR OUT -> D2
    Exit  IR OUT -> D3
    Relay IN     -> D8 (optional)
    Sensors: VCC->5V, GND->GND

  Notes:
  - Many IR modules output LOW when triggered. Set SENSOR_ACTIVE_LOW accordingly.
  - Debounce + lockout prevents double counts.
*/

const int ENTRY_PIN = 2;
const int EXIT_PIN  = 3;
const int RELAY_PIN = 8;   // optional

// ---- Behavior toggles ----
const bool SENSOR_ACTIVE_LOW = true;   // true if sensor outputs LOW when it detects an object
const bool USE_RELAY = true;           // set false if you don't have a relay

// ---- Counting rules ----
volatile int occupancy = 0;
const int CAPACITY_LIMIT = 200;        // change to your mosque/room capacity

// ---- Timing to avoid false/double triggers ----
const unsigned long DEBOUNCE_MS = 60;      // filters noisy signals
const unsigned long LOCKOUT_MS  = 800;     // prevents multiple counts from one person

// Internal state
int entryStable = HIGH;
int exitStable  = HIGH;

int lastEntryRaw = HIGH;
int lastExitRaw  = HIGH;

unsigned long lastEntryChange = 0;
unsigned long lastExitChange  = 0;

unsigned long lastEntryCountTime = 0;
unsigned long lastExitCountTime  = 0;

// Helpers
bool isTriggered(int stableLevel) {
  // If active-low: triggered when LOW
  return SENSOR_ACTIVE_LOW ? (stableLevel == LOW) : (stableLevel == HIGH);
}

void updateRelay() {
  if (!USE_RELAY) return;

  // Example rule: relay ON when occupancy >= CAPACITY_LIMIT
  if (occupancy >= CAPACITY_LIMIT) {
    digitalWrite(RELAY_PIN, HIGH);   // some relay boards are active LOW; flip if needed
  } else {
    digitalWrite(RELAY_PIN, LOW);
  }
}

void printStatus(const char* eventLabel) {
  Serial.print("[");
  Serial.print(eventLabel);
  Serial.print("] Occupancy = ");
  Serial.print(occupancy);

  Serial.print(" | Capacity = ");
  Serial.print(CAPACITY_LIMIT);

  if (USE_RELAY) {
    Serial.print(" | Relay = ");
    Serial.println((occupancy >= CAPACITY_LIMIT) ? "ON" : "OFF");
  } else {
    Serial.println();
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(ENTRY_PIN, INPUT_PULLUP);   // works well for many IR modules; change to INPUT if needed
  pinMode(EXIT_PIN,  INPUT_PULLUP);

  if (USE_RELAY) {
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
  }

  // Initialize stable states
  entryStable = digitalRead(ENTRY_PIN);
  exitStable  = digitalRead(EXIT_PIN);

  printStatus("SYSTEM START");
}

void loop() {
  unsigned long now = millis();

  // ---------- Read raw ----------
  int entryRaw = digitalRead(ENTRY_PIN);
  int exitRaw  = digitalRead(EXIT_PIN);

  // ---------- Debounce ENTRY ----------
  if (entryRaw != lastEntryRaw) {
    lastEntryRaw = entryRaw;
    lastEntryChange = now;
  }
  if ((now - lastEntryChange) >= DEBOUNCE_MS) {
    entryStable = entryRaw;
  }

  // ---------- Debounce EXIT ----------
  if (exitRaw != lastExitRaw) {
    lastExitRaw = exitRaw;
    lastExitChange = now;
  }
  if ((now - lastExitChange) >= DEBOUNCE_MS) {
    exitStable = exitRaw;
  }

  // ---------- Count ENTRY trigger ----------
  // Count on trigger edge: when sensor becomes "triggered"
  static bool entryWasTriggered = false;
  bool entryNowTriggered = isTriggered(entryStable);

  if (entryNowTriggered && !entryWasTriggered) {
    // Triggered edge detected
    if ((now - lastEntryCountTime) >= LOCKOUT_MS) {
      occupancy++;
      lastEntryCountTime = now;
      printStatus("ENTER");
      updateRelay();
    }
  }
  entryWasTriggered = entryNowTriggered;

  // ---------- Count EXIT trigger ----------
  static bool exitWasTriggered = false;
  bool exitNowTriggered = isTriggered(exitStable);

  if (exitNowTriggered && !exitWasTriggered) {
    if ((now - lastExitCountTime) >= LOCKOUT_MS) {
      if (occupancy > 0) occupancy--;   // prevent negative
      lastExitCountTime = now;
      printStatus("EXIT");
      updateRelay();
    }
  }
  exitWasTriggered = exitNowTriggered;

  // Optional: small delay to reduce CPU noise
  delay(5);
}
