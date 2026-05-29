# ESP32-C3 BLE Wiggler

![Build](https://github.com/YOUR_USER/YOUR_REPO/actions/workflows/build.yml/badge.svg)

A Bluetooth mouse and keyboard jiggler for the ESP32-C3 SuperMini. It keeps a machine from going idle by nudging the cursor around and typing the odd word, with timing that looks human instead of robotic. Nothing to install on the host. It pairs as a normal Bluetooth input device.

I made this so my work laptop stops dropping to idle during long calls and reading sessions. Power is the only cable it needs. Everything else runs over BLE.

## What it does

- Pairs as a combined mouse and keyboard over BLE
- Moves the cursor along curved paths instead of straight lines, with real acceleration and braking. It hops between a few random points, then heads back to center
- Types into whatever window has focus: a word or a short phrase at 60 to 80 WPM by default, uneven keystrokes, the occasional pause. Then deletes it again
- Waits a random interval between actions (defaults to 10 to 90 seconds)
- Pauses and resumes from the onboard BOOT button
- Shows its state on the onboard LED
- Accepts text commands over serial: pause, resume, status, profile presets, runtime configuration, and a custom word pool
- Persists all settings and custom words to NVS automatically, so changes survive a power cycle
- Supports QWERTY and QWERTZ keyboard layouts
- Logs to serial only when a host has the port open, so it stays quiet on a power bank

## Hardware

- ESP32-C3 SuperMini
- USB-C cable for power and flashing
- A power bank, if you want it off the desk

No wiring. The BOOT button and the LED are already on the board.

## Software setup

Read this part carefully. It is where most people get stuck.

The combo library targets **ESP32 Arduino Core 2.x**. It will not compile on 3.x, because the BLE API switched from `std::string` to Arduino's `String`. Stay on Core **2.0.17**.

1. Add this board manager URL in Preferences:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. In Boards Manager, install "esp32 by Espressif Systems" version **2.0.17**.
3. The combo library is not in the Library Manager. Grab [ESP32-BLE-Combo](https://github.com/blackketter/ESP32-BLE-Combo) as a ZIP and add it through Sketch → Include Library → Add .ZIP Library.

The `Preferences` library used for NVS persistence ships with the ESP32 Core. No extra install.

### Board settings (Tools menu)

| Setting | Value |
|---|---|
| Board | ESP32C3 Dev Module |
| USB CDC On Boot | Enabled |
| CPU Frequency | 160 MHz |
| Flash Mode | QIO |
| Flash Size | 4MB |
| Upload Speed | 921600 |
| JTAG Adapter | Disabled |

Get `USB CDC On Boot` right. Leave it off and the serial monitor over USB stays dead.

## Configuration

Two layers:

**Sketch defaults** (used when no saved settings exist, edit before flashing). Near the top of `esp32_ble_wiggler.ino`:

```cpp
const bool DEFAULT_QWERTZ = true;
const int DEFAULT_WPM_MIN = 60;
const int DEFAULT_WPM_MAX = 80;
const unsigned long DEFAULT_INTERVAL_MIN_SEC = 10;
const unsigned long DEFAULT_INTERVAL_MAX_SEC = 90;
const int DEFAULT_FIELD = 600;
const bool DEFAULT_MOUSE_ENABLED = true;
const bool DEFAULT_KEYBOARD_ENABLED = true;
```

**Runtime settings via serial commands**. All settings and custom words are auto-saved to NVS, see Usage below. Run `reset` to wipe stored values and fall back to sketch defaults.

About the layout default: the wiggler sends key positions over HID, not characters. A QWERTZ host turns `yet` into `zet` because Y and Z sit on swapped keys. With `DEFAULT_QWERTZ = true`, the code swaps y and z before sending. Set to `false` for QWERTY.

## Flashing

Open `esp32_ble_wiggler/esp32_ble_wiggler.ino`, select the port, upload. If it refuses to connect, force the bootloader: hold BOOT, tap RESET (or replug USB), release BOOT, upload again.

When you flash a new version over an existing one, stored settings stay in NVS and are picked up by the new firmware. Run `reset` afterwards if you want a clean slate.

## Pairing

1. Power the board. It advertises after a second or two.
2. On the host: Bluetooth settings, add device, Bluetooth, pick "Logitech Combo".
3. Done. It reconnects on its own from then on.

The name lives in the sketch (`Logitech Combo`). Pick something that won't stand out in a Bluetooth list, reflash, and it updates on the next pairing.

One catch: BLE only advertises while nothing is connected. If one host is already paired, a second device won't see it until you disconnect the first.

## Usage

**BOOT button (GPIO 9):** press to pause, press again to resume. Works mid-movement too. A running action gets cut off cleanly, the cursor moves back, and any half-typed text is deleted first.

**Status LED (GPIO 8):**

| LED | State |
|---|---|
| Short blink every 2 s | Active and connected |
| Fast blinking | Active, waiting for BLE |
| Solid | Paused |

Most SuperMini boards wire this LED inverted (LOW = on), which the code already handles. If yours runs backwards or never lights up, the comments in `setLed()` tell you what to flip.

**Serial commands:** plug the board into a host, open the serial monitor at 115200 baud, type a command, hit Enter.

Control:

| Command | Effect |
|---|---|
| `pause` | Pauses the wiggler |
| `resume` | Resumes (alias: `start`) |
| `toggle` | Switches between active and paused |
| `now` | Triggers the next action immediately, useful for testing |

Info:

| Command | Effect |
|---|---|
| `status` | Prints current state, BLE connection, and all settings |
| `help` | Lists the commands (alias: `?`) |

Configuration (auto-saved to NVS, no arguments shows current value):

| Command | Range | Effect |
|---|---|---|
| `wpm <min> <max>` | 10 to 200 | Typing speed range in WPM |
| `interval <min> <max>` | 1 to 3600 | Delay between actions in seconds |
| `field <size>` | 50 to 2000 | Mouse movement field size in px |
| `layout <qwerty\|qwertz>` | - | Keyboard layout |
| `mouse <on\|off>` | - | Enable/disable mouse actions |
| `keyboard <on\|off>` | - | Enable/disable keyboard actions |
| `reset` | - | Reset all settings and custom words, clear NVS |

Profiles (preset bundles, the layout setting is left untouched because it depends on the host):

| Profile | WPM | Interval | Field | Modes |
|---|---|---|---|---|
| `work` | 60-80 | 30-120 s | 600 | mouse + keyboard |
| `stealth` | 50-70 | 120-300 s | 400 | mouse only |
| `intense` | 80-110 | 5-30 s | 800 | mouse + keyboard |
| `test` | 80-100 | 5-10 s | 400 | mouse + keyboard (for demos) |

Apply with `profile <name>`, list available with `profile` alone.

Custom word pool (auto-saved, up to 20 words/phrases, max 50 chars each):

| Command | Effect |
|---|---|
| `word` or `word list` | Show current custom words with indices |
| `word add <text>` | Add a word or phrase, trailing space is added automatically |
| `word remove <index>` | Remove by index from `word list` |
| `word clear` | Remove all custom words |

When custom words are present, the typing logic uses them roughly one third of the time, mixed with the built-in pool. Useful for adding company- or context-specific terms that blend in better than generic placeholders.

You cannot disable both mouse and keyboard at the same time, the wiggler needs at least one. Commands work mid-action, same as the BOOT button. On a power bank without a host, serial is gone and the button is the only control.

**About NVS writes:** every configuration change writes to flash. NVS uses wear-leveling and the chip handles roughly 100k+ effective writes per cell with this scheme. At realistic usage (a handful of changes per week) that lasts well beyond the lifetime of the board.

## A note on drift

A BLE mouse sends *relative* movement only. The board has no idea where the cursor actually sits. The virtual field keeps drift in check by returning to center each cycle, but it can't cancel it entirely. Two things make it worse:

- Windows pointer acceleration ("Enhance pointer precision"). Turn it off for clean 1:1 movement.
- Screen edges. Hit one and the relative moves get swallowed, and the model drifts away from reality.

For a jiggler this barely matters. The cursor only has to look alive. If it keeps creeping into one corner over a long session, drop the field size with `field 400`.

## Case

I print mine into a case by **i-BoxIt**: [ESP32-C3 SuperMini Case Options](https://makerworld.com/de/models/1072508-esp32-c3-supermini-case-options). It has cutouts for the LEDs and a flexible section that lets you press the button without a separate part. ASA holds up well if it sits near a window.

The case is i-BoxIt's design, not mine. Check the license on the MakerWorld page before you redistribute the model or a remix. The code here and the case are licensed separately, so don't bundle the STL into this repo. Just link to it.

## Continuous integration

A GitHub Actions workflow under `.github/workflows/build.yml` compiles the sketch against ESP32 Core 2.0.17 and the BleCombo library on every push to `main` and on pull requests. The badge at the top of this README reflects the latest build state. Update the badge URL to point at your fork once you publish.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for the version history.

## Disclaimer

This is a hobby project. Whether it's okay to actually run comes down to your workplace, your contract, and local rules. That call is yours, not the code's. Your risk.

## License

The code is MIT, see [LICENSE](LICENSE). The case design is not covered by it and stays with its creator.
