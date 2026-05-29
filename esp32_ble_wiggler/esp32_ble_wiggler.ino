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
 *   Serial commands     - pause, resume, toggle, status, now, help
 *
 * Version: 1.2.0
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

// === KEYBOARD LAYOUT ===
// Set to true if the host computer uses a QWERTZ layout (e.g. German keyboards).
// Set to false for QWERTY (US, UK, most others).
const bool QWERTZ = true;

// === BOOT BUTTON / PAUSE TOGGLE (interrupt based) ===
const int BUTTON_PIN = 9;
volatile bool buttonPressed = false;
bool wigglerActive = true;
unsigned long lastButtonAction = 0;
const unsigned long debounceTime = 250;

// === STATUS LED ===
const int LED_PIN = 8;
bool ledState = false;
unsigned long lastLedToggle = 0;

// === VIRTUAL MOUSE POSITION (600x600 field) ===
float vx = 300.0, vy = 300.0;
const int FIELD = 600;
const int MARGIN = 20;

// === SERIAL COMMAND BUFFER ===
String serialBuffer = "";
const int MAX_CMD_LEN = 32;

void IRAM_ATTR buttonISR() {
  buttonPressed = true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("\n========================================");
  Serial.println("   ESP32-C3 BLE Wiggler v1.2.0");
  Serial.println("========================================");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

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
  Serial.println("Type 'help' for serial commands");
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

  if (connected) {
    unsigned long now = millis();

    // Either the interval has elapsed, or a serial 'now' command forced it
    if (forceAction || now - lastAction >= interval) {
      forceAction = false;
      int action = random(0, 4);

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
      interval = random(10000, 90000);

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
// then dispatches the command. Does not block when input is partial.
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
        // Buffer would overflow: discard and reset
        serialBuffer = "";
      }
    }
  }
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;

  if (cmd == "pause") {
    if (wigglerActive) {
      wigglerActive = false;
      Serial.println(">>> COMMAND: paused\n");
    } else {
      Serial.println(">>> Already paused\n");
    }
  } else if (cmd == "resume" || cmd == "start") {
    if (!wigglerActive) {
      wigglerActive = true;
      Serial.println(">>> COMMAND: resumed\n");
    } else {
      Serial.println(">>> Already active\n");
    }
  } else if (cmd == "toggle") {
    wigglerActive = !wigglerActive;
    Serial.print(">>> COMMAND: toggled, now ");
    Serial.println(wigglerActive ? "ACTIVE\n" : "PAUSED\n");
  } else if (cmd == "status") {
    printStatus();
  } else if (cmd == "now") {
    if (!wigglerActive) {
      Serial.println(">>> Wiggler is paused, resume first\n");
    } else if (!keyboard.isConnected()) {
      Serial.println(">>> No BLE connection, cannot trigger\n");
    } else {
      forceAction = true;
      Serial.println(">>> COMMAND: triggering action now\n");
    }
  } else if (cmd == "help" || cmd == "?") {
    printHelp();
  } else {
    Serial.print(">>> Unknown command: '");
    Serial.print(cmd);
    Serial.println("'");
    Serial.println(">>> Type 'help' for available commands\n");
  }
}

void printStatus() {
  Serial.println("\n>>> Status:");
  Serial.print("    Wiggler: ");
  Serial.println(wigglerActive ? "ACTIVE" : "PAUSED");
  Serial.print("    BLE:     ");
  Serial.println(keyboard.isConnected() ? "CONNECTED" : "DISCONNECTED");
  Serial.print("    Layout:  ");
  Serial.println(QWERTZ ? "QWERTZ" : "QWERTY");
  if (wigglerActive && keyboard.isConnected()) {
    unsigned long now = millis();
    if (lastAction + interval > now) {
      Serial.print("    Next:    in ");
      Serial.print((lastAction + interval - now) / 1000);
      Serial.println("s");
    } else {
      Serial.println("    Next:    pending");
    }
  }
  Serial.println();
}

void printHelp() {
  Serial.println("\n>>> Available commands:");
  Serial.println("    pause   - pause the wiggler");
  Serial.println("    resume  - resume the wiggler (alias: start)");
  Serial.println("    toggle  - toggle active/paused");
  Serial.println("    status  - show current status");
  Serial.println("    now     - trigger an action immediately");
  Serial.println("    help    - show this help (alias: ?)");
  Serial.println();
}

// === MOUSE MOVEMENT ===
void doMouseMovement() {
  int targetCount = random(2, 6);

  Serial.print("  MOUSE: ");
  Serial.print(targetCount);
  Serial.println(" targets in 600x600 field");

  for (int z = 0; z < targetCount; z++) {
    handleButton();
    handleSerial();
    updateLed();
    if (!wigglerActive) break;

    float targetX = random(MARGIN, FIELD - MARGIN);
    float targetY = random(MARGIN, FIELD - MARGIN);

    if (random(0, 10) < 3) {
      float dx = targetX - vx;
      float dy = targetY - vy;
      float overX = constrain(targetX + dx * 0.12, (float)MARGIN, (float)(FIELD - MARGIN));
      float overY = constrain(targetY + dy * 0.12, (float)MARGIN, (float)(FIELD - MARGIN));
      moveTo(overX, overY);
      delay(random(30, 90));
      if (wigglerActive) moveTo(targetX, targetY);
    } else {
      moveTo(targetX, targetY);
    }

    delay(random(80, 400));
  }

  if (wigglerActive) moveTo(FIELD / 2.0, FIELD / 2.0);
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
  if (!QWERTZ) return c;
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

  int wpm = random(60, 81);
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
