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
 *   Serial commands     - type 'help' for the full list
 *
 * Version: 1.3.0
 */

#include <BleCombo.h>

// Device identity as seen in the host's Bluetooth list
BleComboKeyboard keyboard("Logitech Combo", "Logitech", 100);
BleComboMouse mouse(&keyboard);

unsigned long lastAction = 0;
unsigned long interval = 30000;
bool lastConnectionState = false;
bool forceAction = false;

// === SINGLE WORDS ===
const char* words[] = {
  "ok ", "test ", "hello ", "note ", "info ", "check ",
  "todo ", "done ", "sure ", "thanks ", "yet ", "quick ",
  "mail ", "date ", "update ", "status ", "query ", "moment "
};
const int wordCount = 18;

// === SHORT PHRASES ===
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

// === RUNTIME CONFIGURATION DEFAULTS ===
// These are the values used on every boot. To change them permanently,
// edit the constants and reflash. To change them for the current session,
// use the serial commands (see 'help').
const int DEFAULT_WPM_MIN = 60;
const int DEFAULT_WPM_MAX = 80;
const unsigned long DEFAULT_INTERVAL_MIN_SEC = 10;
const unsigned long DEFAULT_INTERVAL_MAX_SEC = 90;
const int DEFAULT_FIELD = 600;
const bool DEFAULT_QWERTZ = true;
const bool DEFAULT_MOUSE_ENABLED = true;
const bool DEFAULT_KEYBOARD_ENABLED = true;

// === RUNTIME SETTINGS (changeable via serial) ===
int wpmMin = DEFAULT_WPM_MIN;
int wpmMax = DEFAULT_WPM_MAX;
unsigned long intervalMinSec = DEFAULT_INTERVAL_MIN_SEC;
unsigned long intervalMaxSec = DEFAULT_INTERVAL_MAX_SEC;
int fieldSize = DEFAULT_FIELD;
bool qwertz = DEFAULT_QWERTZ;
bool mouseEnabled = DEFAULT_MOUSE_ENABLED;
bool keyboardEnabled = DEFAULT_KEYBOARD_ENABLED;

// === BOOT BUTTON / PAUSE TOGGLE ===
const int BUTTON_PIN = 9;
volatile bool buttonPressed = false;
bool wigglerActive = true;
unsigned long lastButtonAction = 0;
const unsigned long debounceTime = 250;

// === STATUS LED ===
const int LED_PIN = 8;
bool ledState = false;
unsigned long lastLedToggle = 0;

// === VIRTUAL MOUSE POSITION ===
float vx, vy;            // initialized in setup based on fieldSize
const int MARGIN = 20;   // safety margin to the field edge

// === SERIAL COMMAND BUFFER ===
String serialBuffer = "";
const int MAX_CMD_LEN = 64;

void IRAM_ATTR buttonISR() {
  buttonPressed = true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("\n========================================");
  Serial.println("   ESP32-C3 BLE Wiggler v1.3.0");
  Serial.println("========================================");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  vx = fieldSize / 2.0;
  vy = fieldSize / 2.0;

  keyboard.begin();
  mouse.begin();
  randomSeed(analogRead(0));

  Serial.println("BLE advertising active");
  Serial.println("Device name: Logitech Combo");
  Serial.print("Layout: ");
  Serial.println(qwertz ? "QWERTZ" : "QWERTY");
  Serial.println("Type 'help' for serial commands, 'status' for current settings");
  Serial.println("Waiting for connection...\n");
}

void loop() {
  handleButton();
  handleSerial();
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

  // Skip if both action modes are disabled (should be prevented by validation)
  if (!mouseEnabled && !keyboardEnabled) {
    delay(20);
    return;
  }

  if (connected) {
    unsigned long now = millis();

    if (forceAction || now - lastAction >= interval) {
      forceAction = false;

      // Decide what to do based on enabled modes
      bool doMouse = false;
      bool doKeyboard = false;

      if (mouseEnabled && keyboardEnabled) {
        int action = random(0, 4); // 0,1 = mouse, 2 = keyboard, 3 = both
        doMouse = (action <= 1 || action == 3);
        doKeyboard = (action == 2 || action == 3);
      } else if (mouseEnabled) {
        doMouse = true;
      } else {
        doKeyboard = true;
      }

      Serial.print("[");
      Serial.print(now / 1000);
      Serial.print("s] Action: ");
      if (doMouse && doKeyboard) Serial.println("Mouse + keyboard");
      else if (doMouse) Serial.println("Mouse only");
      else Serial.println("Keyboard only");

      if (doMouse) {
        doMouseMovement();
      }

      if (doKeyboard && wigglerActive) {
        if (doMouse) delay(random(300, 800));
        typeInEditor();
      }

      lastAction = now;
      interval = random(intervalMinSec * 1000UL, intervalMaxSec * 1000UL + 1);

      Serial.print("Next action in ");
      Serial.print(interval / 1000);
      Serial.println("s\n");
    }
  }
  delay(20);
}

// === LED CONTROL ===
void setLed(bool on) {
  digitalWrite(LED_PIN, on ? LOW : HIGH);
}

void updateLed() {
  if (!wigglerActive) {
    setLed(true);
    ledState = true;
    return;
  }

  unsigned long now = millis();
  unsigned long onTime, offTime;

  if (keyboard.isConnected()) {
    onTime = 80; offTime = 1920;
  } else {
    onTime = 150; offTime = 150;
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

// === BUTTON HANDLER ===
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

// === SERIAL COMMANDS ===
// Non-blocking serial reader: collects characters into a buffer until newline,
// then dispatches the command.
void handleSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        processCommand(serialBuffer);
        serialBuffer = "";
      }
    } else {
      if (serialBuffer.length() < MAX_CMD_LEN) {
        serialBuffer += c;
      } else {
        serialBuffer = "";
      }
    }
  }
}

// Helper: split a string at the first space into two parts.
// Returns true if a space was found.
bool splitAtSpace(String input, String &first, String &second) {
  input.trim();
  int sp = input.indexOf(' ');
  if (sp == -1) {
    first = input;
    second = "";
    return false;
  }
  first = input.substring(0, sp);
  second = input.substring(sp + 1);
  first.trim();
  second.trim();
  return true;
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;

  // Split command into name and arguments
  String name, args;
  splitAtSpace(cmd, name, args);

  if (name == "pause") {
    if (wigglerActive) {
      wigglerActive = false;
      Serial.println(">>> COMMAND: paused\n");
    } else {
      Serial.println(">>> Already paused\n");
    }
  } else if (name == "resume" || name == "start") {
    if (!wigglerActive) {
      wigglerActive = true;
      Serial.println(">>> COMMAND: resumed\n");
    } else {
      Serial.println(">>> Already active\n");
    }
  } else if (name == "toggle") {
    wigglerActive = !wigglerActive;
    Serial.print(">>> COMMAND: toggled, now ");
    Serial.println(wigglerActive ? "ACTIVE\n" : "PAUSED\n");
  } else if (name == "status") {
    printStatus();
  } else if (name == "now") {
    if (!wigglerActive) {
      Serial.println(">>> Wiggler is paused, resume first\n");
    } else if (!keyboard.isConnected()) {
      Serial.println(">>> No BLE connection, cannot trigger\n");
    } else if (!mouseEnabled && !keyboardEnabled) {
      Serial.println(">>> Both modes disabled, enable at least one\n");
    } else {
      forceAction = true;
      Serial.println(">>> COMMAND: triggering action now\n");
    }
  } else if (name == "help" || name == "?") {
    printHelp();
  } else if (name == "reset") {
    resetDefaults();
    Serial.println(">>> Settings reset to defaults\n");
  } else if (name == "wpm") {
    cmdWpm(args);
  } else if (name == "interval") {
    cmdInterval(args);
  } else if (name == "field") {
    cmdField(args);
  } else if (name == "layout") {
    cmdLayout(args);
  } else if (name == "mouse") {
    cmdMouse(args);
  } else if (name == "keyboard") {
    cmdKeyboard(args);
  } else {
    Serial.print(">>> Unknown command: '");
    Serial.print(name);
    Serial.println("'");
    Serial.println(">>> Type 'help' for available commands\n");
  }
}

// === CONFIG COMMANDS ===
void cmdWpm(String args) {
  if (args.length() == 0) {
    Serial.print(">>> WPM: ");
    Serial.print(wpmMin);
    Serial.print(" to ");
    Serial.println(wpmMax);
    return;
  }
  String a, b;
  if (!splitAtSpace(args, a, b)) {
    Serial.println(">>> Usage: wpm <min> <max>  (e.g. wpm 60 80)\n");
    return;
  }
  int newMin = a.toInt();
  int newMax = b.toInt();
  if (newMin < 10 || newMax > 200 || newMin > newMax) {
    Serial.println(">>> Invalid range. Use 10-200, with min <= max\n");
    return;
  }
  wpmMin = newMin;
  wpmMax = newMax;
  Serial.print(">>> WPM set to ");
  Serial.print(wpmMin);
  Serial.print(" - ");
  Serial.println(wpmMax);
  Serial.println();
}

void cmdInterval(String args) {
  if (args.length() == 0) {
    Serial.print(">>> Interval: ");
    Serial.print(intervalMinSec);
    Serial.print(" to ");
    Serial.print(intervalMaxSec);
    Serial.println(" s");
    return;
  }
  String a, b;
  if (!splitAtSpace(args, a, b)) {
    Serial.println(">>> Usage: interval <min> <max>  (in seconds, e.g. interval 10 90)\n");
    return;
  }
  long newMin = a.toInt();
  long newMax = b.toInt();
  if (newMin < 1 || newMax > 3600 || newMin > newMax) {
    Serial.println(">>> Invalid range. Use 1-3600 seconds, with min <= max\n");
    return;
  }
  intervalMinSec = (unsigned long)newMin;
  intervalMaxSec = (unsigned long)newMax;
  Serial.print(">>> Interval set to ");
  Serial.print(intervalMinSec);
  Serial.print(" - ");
  Serial.print(intervalMaxSec);
  Serial.println(" s\n");
}

void cmdField(String args) {
  if (args.length() == 0) {
    Serial.print(">>> Field: ");
    Serial.print(fieldSize);
    Serial.print(" x ");
    Serial.print(fieldSize);
    Serial.println(" px");
    return;
  }
  int newSize = args.toInt();
  if (newSize < 50 || newSize > 2000) {
    Serial.println(">>> Invalid size. Use 50-2000 px\n");
    return;
  }
  fieldSize = newSize;
  // Clamp the virtual position so it stays inside the new field
  vx = constrain(vx, (float)MARGIN, (float)(fieldSize - MARGIN));
  vy = constrain(vy, (float)MARGIN, (float)(fieldSize - MARGIN));
  Serial.print(">>> Field set to ");
  Serial.print(fieldSize);
  Serial.print(" x ");
  Serial.print(fieldSize);
  Serial.println(" px\n");
}

void cmdLayout(String args) {
  if (args.length() == 0) {
    Serial.print(">>> Layout: ");
    Serial.println(qwertz ? "QWERTZ" : "QWERTY");
    return;
  }
  if (args == "qwertz") {
    qwertz = true;
    Serial.println(">>> Layout set to QWERTZ\n");
  } else if (args == "qwerty") {
    qwertz = false;
    Serial.println(">>> Layout set to QWERTY\n");
  } else {
    Serial.println(">>> Usage: layout <qwerty|qwertz>\n");
  }
}

void cmdMouse(String args) {
  if (args.length() == 0) {
    Serial.print(">>> Mouse: ");
    Serial.println(mouseEnabled ? "ON" : "OFF");
    return;
  }
  if (args == "on") {
    mouseEnabled = true;
    Serial.println(">>> Mouse: ON\n");
  } else if (args == "off") {
    if (!keyboardEnabled) {
      Serial.println(">>> Cannot disable mouse, keyboard is already off. Enable keyboard first.\n");
      return;
    }
    mouseEnabled = false;
    Serial.println(">>> Mouse: OFF\n");
  } else {
    Serial.println(">>> Usage: mouse <on|off>\n");
  }
}

void cmdKeyboard(String args) {
  if (args.length() == 0) {
    Serial.print(">>> Keyboard: ");
    Serial.println(keyboardEnabled ? "ON" : "OFF");
    return;
  }
  if (args == "on") {
    keyboardEnabled = true;
    Serial.println(">>> Keyboard: ON\n");
  } else if (args == "off") {
    if (!mouseEnabled) {
      Serial.println(">>> Cannot disable keyboard, mouse is already off. Enable mouse first.\n");
      return;
    }
    keyboardEnabled = false;
    Serial.println(">>> Keyboard: OFF\n");
  } else {
    Serial.println(">>> Usage: keyboard <on|off>\n");
  }
}

void resetDefaults() {
  wpmMin = DEFAULT_WPM_MIN;
  wpmMax = DEFAULT_WPM_MAX;
  intervalMinSec = DEFAULT_INTERVAL_MIN_SEC;
  intervalMaxSec = DEFAULT_INTERVAL_MAX_SEC;
  fieldSize = DEFAULT_FIELD;
  qwertz = DEFAULT_QWERTZ;
  mouseEnabled = DEFAULT_MOUSE_ENABLED;
  keyboardEnabled = DEFAULT_KEYBOARD_ENABLED;
  vx = fieldSize / 2.0;
  vy = fieldSize / 2.0;
}

// === STATUS / HELP ===
void printStatus() {
  Serial.println("\n>>> Status:");
  Serial.print("  Wiggler:  ");
  Serial.println(wigglerActive ? "ACTIVE" : "PAUSED");
  Serial.print("  BLE:      ");
  Serial.println(keyboard.isConnected() ? "CONNECTED" : "DISCONNECTED");
  Serial.print("  Layout:   ");
  Serial.println(qwertz ? "QWERTZ" : "QWERTY");
  Serial.print("  Mouse:    ");
  Serial.println(mouseEnabled ? "ON" : "OFF");
  Serial.print("  Keyboard: ");
  Serial.println(keyboardEnabled ? "ON" : "OFF");
  Serial.print("  WPM:      ");
  Serial.print(wpmMin);
  Serial.print(" - ");
  Serial.println(wpmMax);
  Serial.print("  Interval: ");
  Serial.print(intervalMinSec);
  Serial.print(" - ");
  Serial.print(intervalMaxSec);
  Serial.println(" s");
  Serial.print("  Field:    ");
  Serial.print(fieldSize);
  Serial.print(" x ");
  Serial.print(fieldSize);
  Serial.println(" px");
  if (wigglerActive && keyboard.isConnected()) {
    unsigned long now = millis();
    if (lastAction + interval > now) {
      Serial.print("  Next:     in ");
      Serial.print((lastAction + interval - now) / 1000);
      Serial.println(" s");
    } else {
      Serial.println("  Next:     pending");
    }
  }
  Serial.println();
}

void printHelp() {
  Serial.println("\n>>> Available commands:");
  Serial.println();
  Serial.println("  Control:");
  Serial.println("    pause                  - pause the wiggler");
  Serial.println("    resume                 - resume (alias: start)");
  Serial.println("    toggle                 - toggle active/paused");
  Serial.println("    now                    - trigger next action immediately");
  Serial.println();
  Serial.println("  Info:");
  Serial.println("    status                 - show current state and settings");
  Serial.println("    help                   - show this help (alias: ?)");
  Serial.println();
  Serial.println("  Configuration:");
  Serial.println("    wpm <min> <max>        - typing speed in WPM (10-200)");
  Serial.println("    interval <min> <max>   - delay between actions in seconds (1-3600)");
  Serial.println("    field <size>           - mouse field size in px (50-2000)");
  Serial.println("    layout <qwerty|qwertz> - keyboard layout");
  Serial.println("    mouse <on|off>         - enable/disable mouse actions");
  Serial.println("    keyboard <on|off>      - enable/disable keyboard actions");
  Serial.println("    reset                  - reset all settings to defaults");
  Serial.println();
  Serial.println("  Configuration commands without arguments show the current value.");
  Serial.println("  Runtime changes are lost on power cycle, edit DEFAULT_* constants for permanent changes.");
  Serial.println();
}

// === MOUSE MOVEMENT ===
void doMouseMovement() {
  int targetCount = random(2, 6);

  Serial.print("  MOUSE: ");
  Serial.print(targetCount);
  Serial.print(" targets in ");
  Serial.print(fieldSize);
  Serial.print("x");
  Serial.print(fieldSize);
  Serial.println(" field");

  for (int z = 0; z < targetCount; z++) {
    handleButton();
    handleSerial();
    updateLed();
    if (!wigglerActive) break;

    float targetX = random(MARGIN, fieldSize - MARGIN);
    float targetY = random(MARGIN, fieldSize - MARGIN);

    if (random(0, 10) < 3) {
      float dx = targetX - vx;
      float dy = targetY - vy;
      float overX = constrain(targetX + dx * 0.12, (float)MARGIN, (float)(fieldSize - MARGIN));
      float overY = constrain(targetY + dy * 0.12, (float)MARGIN, (float)(fieldSize - MARGIN));
      moveTo(overX, overY);
      delay(random(30, 90));
      if (wigglerActive) moveTo(targetX, targetY);
    } else {
      moveTo(targetX, targetY);
    }

    delay(random(80, 400));
  }

  if (wigglerActive) moveTo(fieldSize / 2.0, fieldSize / 2.0);
}

void moveTo(float targetX, float targetY) {
  float startX = vx;
  float startY = vy;

  float dx = targetX - startX;
  float dy = targetY - startY;
  float dist = sqrt(dx * dx + dy * dy);
  if (dist < 1.0) return;

  float midX = (startX + targetX) / 2.0;
  float midY = (startY + targetY) / 2.0;
  float offset = random(-40, 41) / 100.0 * dist;
  float ctrlX = midX + (-dy / dist) * offset;
  float ctrlY = midY + ( dx / dist) * offset;

  int steps = constrain((int)(dist / 8.0), 12, 60);

  int speedMin, speedMax;
  if (random(0, 10) < 5) { speedMin = 2; speedMax = 7; }
  else                   { speedMin = 5; speedMax = 14; }

  float prevX = startX;
  float prevY = startY;

  for (int i = 1; i <= steps; i++) {
    handleButton();
    handleSerial();
    updateLed();
    if (!wigglerActive) break;

    float t = (float)i / steps;
    float te = (1 - cos(t * PI)) / 2;

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

  vx = prevX;
  vy = prevY;
}

// === KEYBOARD ===
char remapForLayout(char c) {
  if (!qwertz) return c;
  if (c == 'y') return 'z';
  if (c == 'z') return 'y';
  if (c == 'Y') return 'Z';
  if (c == 'Z') return 'Y';
  return c;
}

int humanDelay(int baseDelay, bool withPauses) {
  int variation = random(-baseDelay / 3, baseDelay / 3 + 1);
  int d = baseDelay + variation;
  if (withPauses && random(0, 12) == 0) {
    d += random(120, 350);
  }
  if (d < 40) d = 40;
  return d;
}

void typeInEditor() {
  const char* text;
  if (random(0, 10) < 4) {
    text = phrases[random(0, phraseCount)];
  } else {
    text = words[random(0, wordCount)];
  }
  int len = strlen(text);

  // Pace for this session: random within the configured WPM range
  int wpm = random(wpmMin, wpmMax + 1);
  int baseDelay = 60000 / (wpm * 5);
  int eraseDelay = baseDelay * 0.7;

  Serial.print("  KEY: \"");
  Serial.print(text);
  Serial.print("\" @ ");
  Serial.print(wpm);
  Serial.println(" WPM");

  int typed = 0;
  for (int i = 0; i < len; i++) {
    handleButton();
    handleSerial();
    updateLed();
    if (!wigglerActive) break;
    keyboard.write(remapForLayout(text[i]));
    typed++;
    delay(humanDelay(baseDelay, true));
  }

  delay(random(300, 800));

  for (int i = 0; i < typed; i++) {
    updateLed();
    keyboard.write(KEY_BACKSPACE);
    delay(humanDelay(eraseDelay, false));
  }

  Serial.println("  KEY: erased with backspace");
}
