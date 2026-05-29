/*
 * ESP32-C3 BLE Wiggler
 * A Bluetooth Low Energy mouse + keyboard combo that keeps a computer
 * marked as active. Randomized, human-like mouse paths and typing.
 *
 * Board:   ESP32-C3 SuperMini
 * Library: ESP32-BLE-Combo (blackketter fork of T-vK's BLE libraries)
 * Core:    ESP32 Arduino Core 2.0.17 (3.x is not compatible, see README)
 *
 * Controls:
 *   BOOT button (GPIO 9) - pause / resume at any time
 *   Onboard LED (GPIO 8) - status indicator (heartbeat = active, solid = paused)
 *
 * Version: 1.1.0
 */

#include <BleCombo.h>

// Device identity as seen in the host's Bluetooth list
BleComboKeyboard keyboard("Logitech Combo", "Logitech", 100);
BleComboMouse mouse(&keyboard);

unsigned long lastAction = 0;
unsigned long interval = 30000;
bool lastConnectionState = false;

// === SINGLE WORDS ===
const char* words[] = {
  "ok ", "test ", "hello ", "note ", "info ", "check ",
  "todo ", "done ", "sure ", "thanks ", "yet ", "quick ",
  "mail ", "date ", "update ", "status ", "query ", "moment "
};
const int wordCount = 18;

// === SHORT PHRASES (3-4 words, like real notes) ===
const char* phrases[] = {
  "todo check mail ",
  "call back later ",
  "review the doc ",
  "send report soon ",
  "follow up with team ",
  "update the file now ",
  "check status before lunch ",
  "note this for later ",
  "ping me back today ",
  "meeting moved to three ",
  "need to confirm date ",
  "draft is almost done "
};
const int phraseCount = 12;

// === KEYBOARD LAYOUT ===
// Set to true if the host computer uses a QWERTZ layout (e.g. German keyboards).
// This swaps y/z before sending so the output matches what you actually want.
// Set to false for QWERTY (US/UK and most other layouts).
const bool QWERTZ = true;

// === BOOT BUTTON / PAUSE TOGGLE (interrupt based) ===
const int BUTTON_PIN = 9;                 // GPIO 9 = BOOT button on the C3 SuperMini
volatile bool buttonPressed = false;      // set inside the ISR
bool wigglerActive = true;                // active by default
unsigned long lastButtonAction = 0;
const unsigned long debounceTime = 250;   // debounce in ms

// === STATUS LED ===
const int LED_PIN = 8;                     // GPIO 8 = onboard LED (inverted!)
bool ledState = false;
unsigned long lastLedToggle = 0;

// === VIRTUAL MOUSE POSITION (600x600 field) ===
float vx = 300.0, vy = 300.0;    // start at field center
const int FIELD = 600;           // movement range in px
const int MARGIN = 20;           // safety margin to the field edge

// Interrupt service routine: only sets a flag, nothing else
void IRAM_ATTR buttonISR() {
  buttonPressed = true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("\n========================================");
  Serial.println("   ESP32-C3 BLE Wiggler v1.1.0");
  Serial.println("========================================");

  // BOOT button as input with pull-up, interrupt on falling edge
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  // Status LED
  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  keyboard.begin();
  mouse.begin();
  randomSeed(analogRead(0));

  Serial.println("BLE advertising active");
  Serial.println("Device name: Logitech Combo");
  Serial.print("Keyboard layout: ");
  Serial.println(QWERTZ ? "QWERTZ (y/z swapped)" : "QWERTY");
  Serial.println("BOOT button (GPIO 9): pause anytime");
  Serial.println("Status LED (GPIO 8): heartbeat = active, solid = paused");
  Serial.println("Waiting for connection...\n");
}

void loop() {
  handleButton();
  updateLed();

  bool connected = keyboard.isConnected();
  if (connected != lastConnectionState) {
    if (connected) {
      Serial.print(">>> STATUS: BLE CONNECTED");
      Serial.println(wigglerActive ? "" : " (paused)");
    } else {
      Serial.println(">>> STATUS: BLE DISCONNECTED");
    }
    lastConnectionState = connected;
  }

  if (!wigglerActive) {
    delay(20);
    return;
  }

  if (connected) {
    unsigned long now = millis();

    if (now - lastAction >= interval) {
      int action = random(0, 4); // 0,1 = mouse, 2 = keyboard, 3 = both

      Serial.print("[");
      Serial.print(now / 1000);
      Serial.print("s] Action: ");
      if (action <= 1) Serial.println("Mouse only");
      else if (action == 2) Serial.println("Keyboard only");
      else Serial.println("Mouse + keyboard");

      if (action <= 1 || action == 3) {
        doMouseMovement();
      }

      if ((action == 2 || action == 3) && wigglerActive) {
        delay(random(300, 800));
        typeInEditor();
      }

      lastAction = now;
      interval = random(10000, 90000); // next action in 10-90 s

      Serial.print("Next action in ");
      Serial.print(interval / 1000);
      Serial.println("s\n");
    }
  }
  delay(20);
}

// === LED CONTROL ===
// The C3 SuperMini onboard LED is inverted: LOW = on, HIGH = off
void setLed(bool on) {
  digitalWrite(LED_PIN, on ? LOW : HIGH);
}

// Non-blocking LED update, called at every wait point
void updateLed() {
  if (!wigglerActive) {
    setLed(true); // paused: solid on
    ledState = true;
    return;
  }

  unsigned long now = millis();
  unsigned long onTime, offTime;

  if (keyboard.isConnected()) {
    onTime = 80; offTime = 1920;   // heartbeat: short blink every 2 s
  } else {
    onTime = 150; offTime = 150;   // fast blink: waiting for BLE
  }

  if (ledState && (now - lastLedToggle >= onTime)) {
    ledState = false;
    setLed(false);
    lastLedToggle = now;
  } else if (!ledState && (now - lastLedToggle >= offTime)) {
    ledState = true;
    setLed(true);
    lastLedToggle = now;
  }
}

// Evaluates the ISR flag with debouncing
void handleButton() {
  if (buttonPressed) {
    buttonPressed = false;
    if (millis() - lastButtonAction > debounceTime) {
      wigglerActive = !wigglerActive;
      Serial.print("\n>>> BOOT button - wiggler is now: ");
      Serial.println(wigglerActive ? "ACTIVE" : "PAUSED");
      Serial.println();
      lastButtonAction = millis();
    }
  }
}

// === MOUSE MOVEMENT ===
// Visits several random targets in the field along curved paths
void doMouseMovement() {
  int targetCount = random(2, 6); // 2 to 5 targets per sequence

  Serial.print("  MOUSE: ");
  Serial.print(targetCount);
  Serial.println(" targets in 600x600 field");

  for (int z = 0; z < targetCount; z++) {
    handleButton();
    updateLed();
    if (!wigglerActive) break;

    // Random target somewhere in the field
    float targetX = random(MARGIN, FIELD - MARGIN);
    float targetY = random(MARGIN, FIELD - MARGIN);

    // About 30%: overshoot the target and correct back
    if (random(0, 10) < 3) {
      float dx = targetX - vx;
      float dy = targetY - vy;
      float overX = constrain(targetX + dx * 0.12, (float)MARGIN, (float)(FIELD - MARGIN));
      float overY = constrain(targetY + dy * 0.12, (float)MARGIN, (float)(FIELD - MARGIN));
      moveTo(overX, overY);              // overshoot first
      delay(random(30, 90));
      if (wigglerActive) moveTo(targetX, targetY); // correct back
    } else {
      moveTo(targetX, targetY);
    }

    // Short dwell at the target
    delay(random(80, 400));
  }

  // Return to field center to counter drift
  if (wigglerActive) moveTo(FIELD / 2.0, FIELD / 2.0);
}

// Moves from the current virtual position to the target along a
// quadratic Bezier curve (curved, not linear).
void moveTo(float targetX, float targetY) {
  float startX = vx;
  float startY = vy;

  float dx = targetX - startX;
  float dy = targetY - startY;
  float dist = sqrt(dx * dx + dy * dy);
  if (dist < 1.0) return; // already there

  // Control point: midpoint of the path, offset perpendicular for the curve
  float midX = (startX + targetX) / 2.0;
  float midY = (startY + targetY) / 2.0;
  float offset = random(-40, 41) / 100.0 * dist; // up to ~40% of the distance
  float ctrlX = midX + (-dy / dist) * offset;
  float ctrlY = midY + ( dx / dist) * offset;

  // Step count proportional to distance for a smooth curve
  int steps = constrain((int)(dist / 8.0), 12, 60);

  // Speed: sometimes a fast flick, sometimes moderate
  int speedMin, speedMax;
  if (random(0, 10) < 5) { speedMin = 2; speedMax = 7; }   // fast
  else                   { speedMin = 5; speedMax = 14; }  // moderate

  float prevX = startX;
  float prevY = startY;

  for (int i = 1; i <= steps; i++) {
    handleButton();
    updateLed();
    if (!wigglerActive) break;

    float t = (float)i / steps;
    float te = (1 - cos(t * PI)) / 2; // ease-in-out

    // Quadratic Bezier interpolation
    float u = 1 - te;
    float px = u * u * startX + 2 * u * te * ctrlX + te * te * targetX;
    float py = u * u * startY + 2 * u * te * ctrlY + te * te * targetY;

    int stepX = round(px - prevX);
    int stepY = round(py - prevY);

    if (stepX != 0 || stepY != 0) {
      mouse.move(stepX, stepY);
      prevX += stepX;
      prevY += stepY;
    }
    delay(random(speedMin, speedMax));
  }

  // Update virtual position to the point actually reached
  vx = prevX;
  vy = prevY;
}

// === KEYBOARD ===
// Swaps y and z when the host uses a QWERTZ layout.
// Other QWERTY/QWERTZ differences (special chars, umlauts) are not handled,
// but the word pool only uses lowercase letters and spaces, so this is enough.
char remapForLayout(char c) {
  if (!QWERTZ) return c;
  if (c == 'y') return 'z';
  if (c == 'z') return 'y';
  if (c == 'Y') return 'Z';
  if (c == 'Z') return 'Y';
  return c;
}

// Returns a realistic keystroke delay in ms.
// baseDelay = base pace, withPauses adds occasional longer pauses.
int humanDelay(int baseDelay, bool withPauses) {
  int variation = random(-baseDelay / 3, baseDelay / 3 + 1);
  int d = baseDelay + variation;
  if (withPauses && random(0, 12) == 0) {
    d += random(120, 350); // occasional thinking pause
  }
  if (d < 40) d = 40;
  return d;
}

void typeInEditor() {
  const char* text;
  // 40% chance for a longer phrase
  if (random(0, 10) < 4) {
    text = phrases[random(0, phraseCount)];
  } else {
    text = words[random(0, wordCount)];
  }
  int len = strlen(text);

  // Pace for this session: 60 to 80 WPM
  // 1 word = 5 keystrokes, base delay = 60000 / (WPM * 5) ms
  int wpm = random(60, 81);
  int baseDelay = 60000 / (wpm * 5);   // 200 ms (60 WPM) to 150 ms (80 WPM)
  int eraseDelay = baseDelay * 0.7;    // erasing is a bit quicker

  Serial.print("  KEY: \"");
  Serial.print(text);
  Serial.print("\" @ ");
  Serial.print(wpm);
  Serial.println(" WPM");

  int typed = 0;
  for (int i = 0; i < len; i++) {
    handleButton();
    updateLed();
    if (!wigglerActive) break;
    keyboard.write(remapForLayout(text[i])); // remap before sending
    typed++;
    delay(humanDelay(baseDelay, true)); // typing: with thinking pauses
  }

  delay(random(300, 800));

  // Erase exactly as many characters as were typed
  for (int i = 0; i < typed; i++) {
    updateLed();
    keyboard.write(KEY_BACKSPACE);
    delay(humanDelay(eraseDelay, false)); // erasing: steadier, no pauses
  }

  Serial.println("  KEY: erased with backspace");
}
