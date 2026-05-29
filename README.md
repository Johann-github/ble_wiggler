# ESP32-C3 BLE Wiggler

A Bluetooth mouse and keyboard jiggler for the ESP32-C3 SuperMini. It keeps a machine from going idle by nudging the cursor around and typing the odd word, with timing that looks human instead of robotic. Nothing to install on the host. It pairs as a normal Bluetooth input device.

I made this so my work laptop stops dropping to idle during long calls and reading sessions. Power is the only cable it needs. Everything else runs over BLE.

## What it does

- Pairs as a combined mouse and keyboard over BLE
- Moves the cursor along curved paths instead of straight lines, with real acceleration and braking. It hops between a few random points, then heads back to center
- Types into whatever window has focus: a word or a short phrase at 60 to 80 WPM by default, uneven keystrokes, the occasional pause. Then deletes it again
- Waits a random interval between actions (defaults to 10 to 90 seconds)
- Pauses and resumes from the onboard BOOT button
- Shows its state on the onboard LED
- Accepts text commands over serial: pause, resume, status, and a full set of runtime configuration commands. WPM, interval, field size, layout, and per-mode toggles can all be changed live without reflashing
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

There are two layers:

**Sketch defaults** (used on every boot, edit before flashing). Near the top of `esp32_ble_wiggler.ino`:

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

**Runtime settings** (changed via serial commands, see Usage). Everything in the list above can be modified live without reflashing. Runtime changes are not saved across power cycles. For permanent changes, edit the defaults and reflash, or use `reset` over serial to restore the defaults during a session.

About the layout default: the wiggler sends key positions over HID, not characters. A QWERTZ host turns `yet` into `zet` because Y and Z sit on swapped keys. With `DEFAULT_QWERTZ = true`, the code swaps y and z before sending. Set to `false` for QWERTY.

## Flashing

Open `esp32_ble_wiggler/esp32_ble_wiggler.ino`, select the port, upload. If it refuses to connect, force the bootloader: hold BOOT, tap RESET (or replug USB), release BOOT, upload again.

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

**Serial commands:** plug the board into a host, open the serial monitor at 115200 baud, type a command, hit Enter. Useful when the board sits where the button is hard to reach, and for changing the behavior without reflashing.

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

Configuration (without arguments these show the current value):

| Command | Range | Effect |
|---|---|---|
| `wpm <min> <max>` | 10 to 200 | Typing speed range in WPM |
| `interval <min> <max>` | 1 to 3600 | Delay between actions in seconds |
| `field <size>` | 50 to 2000 | Mouse movement field size in px |
| `layout <qwerty\|qwertz>` | - | Keyboard layout |
| `mouse <on\|off>` | - | Enable/disable mouse actions |
| `keyboard <on\|off>` | - | Enable/disable keyboard actions |
| `reset` | - | Reset all settings to sketch defaults |

You cannot disable both mouse and keyboard at the same time, the wiggler needs at least one. Commands work mid-action, same as the BOOT button. On a power bank without a host, serial is gone and the button is the only control.

## A note on drift

A BLE mouse sends *relative* movement only. The board has no idea where the cursor actually sits. The virtual field keeps drift in check by returning to center each cycle, but it can't cancel it entirely. Two things make it worse:

- Windows pointer acceleration ("Enhance pointer precision"). Turn it off for clean 1:1 movement.
- Screen edges. Hit one and the relative moves get swallowed, and the model drifts away from reality.

For a jiggler this barely matters. The cursor only has to look alive. If it keeps creeping into one corner over a long session, drop the field size with `field 400`.

## Case

I print mine into a case by **i-BoxIt**: [ESP32-C3 SuperMini Case Options](https://makerworld.com/de/models/1072508-esp32-c3-supermini-case-options). It has cutouts for the LEDs and a flexible section that lets you press the button without a separate part. ASA holds up well if it sits near a window.

The case is i-BoxIt's design, not mine. Check the license on the MakerWorld page before you redistribute the model or a remix. The code here and the case are licensed separately, so don't bundle the STL into this repo. Just link to it.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for the version history.

## Disclaimer

This is a hobby project. Whether it's okay to actually run comes down to your workplace, your contract, and local rules. That call is yours, not the code's. Your risk.

## License

The code is MIT, see [LICENSE](LICENSE). The case design is not covered by it and stays with its creator.
