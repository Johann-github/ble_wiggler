# Changelog

All notable changes to this project are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.5.0] - 2026-05-29

### Added
- **Profile presets** via `profile <name>` command. Four predefined bundles:
  - `work`: 60-80 WPM, 30-120 s interval, both modes
  - `stealth`: 50-70 WPM, 120-300 s interval, mouse only, smaller field
  - `intense`: 80-110 WPM, 5-30 s interval, both modes, larger field
  - `test`: 5-10 s interval for demos
  - `profile` alone lists all available profiles
- **Custom word pool** managed via `word` subcommands:
  - `word list` shows current custom words with indices
  - `word add <text>` adds a word or phrase (auto-appends trailing space)
  - `word remove <index>` removes by index
  - `word clear` wipes the pool
  - Limit: 20 entries, max 50 characters each
  - When custom words exist, typing logic uses them roughly one third of the time
- **GitHub Actions CI**: `.github/workflows/build.yml` compiles the sketch against ESP32 Core 2.0.17 and the BleCombo library on every push and PR
- README now includes a build status badge (update the URL to your fork)

### Changed
- `reset` now also clears the custom word pool, not just settings.
- Profile application does not change the `qwertz` layout setting, since the right layout depends on the host hardware, not the use case.
- Status output reports the number of custom words in the pool.

### Notes
- Custom words are stored in NVS as individual keys (`w0`, `w1`, ...) plus a `wordCount` index. They survive power cycles like all other settings.
- Profiles persist via the same NVS write path as individual settings, so an applied profile is restored on the next boot.

## [1.4.0] - 2026-05-29

### Added
- Persistent settings via NVS (non-volatile storage). All configuration changes through serial commands are automatically written to flash and restored on the next boot. No new commands required, persistence is transparent.
- Boot message reports whether settings were restored from NVS or defaults are in use.
- `status` output now shows that settings are persisted.

### Changed
- The `reset` command now also clears the NVS namespace, so the next boot starts fresh on defaults.
- Help text updated to indicate that configuration commands auto-save.

### Notes
- Uses the `Preferences` library that ships with the ESP32 Arduino Core, no extra installs.
- Stored keys live in the `wiggler` namespace.
- Updating from v1.3.0 keeps previously made runtime settings only after they have been re-entered, since v1.3.0 did not write to NVS.

## [1.3.0] - 2026-05-29

### Added
- Runtime configuration via serial commands. All key behaviors can be changed without reflashing:
  - `wpm <min> <max>` sets the typing speed range (10 to 200 WPM)
  - `interval <min> <max>` sets the delay between actions in seconds (1 to 3600)
  - `field <size>` sets the mouse movement field size in pixels (50 to 2000)
  - `layout <qwerty|qwertz>` switches the keyboard layout live
  - `mouse <on|off>` and `keyboard <on|off>` enable or disable each action type independently
  - `reset` restores all settings to the sketch defaults
- Configuration commands without arguments print the current value.
- `status` now lists all settings plus the BLE state and time until the next action.
- `DEFAULT_*` constants at the top of the sketch define the boot-time values.

### Changed
- `QWERTZ` is no longer a constant. It's a regular variable with `DEFAULT_QWERTZ` as its starting value.
- The `FIELD` constant is replaced by the `fieldSize` variable with `DEFAULT_FIELD` as its starting value.
- Mode selection in the main loop respects the `mouseEnabled` and `keyboardEnabled` flags.

### Notes
- Runtime changes are not persisted in this version. v1.4.0 adds NVS persistence.
- Disabling both modes at once is prevented.

## [1.2.0] - 2026-05-29

### Added
- Serial command interface for runtime control without touching the BOOT button. Commands: `pause`, `resume` (alias `start`), `toggle`, `status`, `now`, `help` (alias `?`).
- `now` command forces the next action immediately.
- `status` command prints wiggler state, BLE connection, active layout, and time until the next action.
- Boot message now mentions the `help` command.

### Changed
- Serial commands and button input are evaluated at every wait point inside the mouse and keyboard loops.
- Internal `forceAction` flag added to cleanly trigger the next action on demand.

## [1.1.0] - 2026-05-29

### Added
- QWERTZ keyboard layout support via a single `QWERTZ` constant near the top of the sketch.
- Layout is logged to serial on boot.

### Changed
- Default value of `QWERTZ` is `true`. Flip to `false` for QWERTY hosts.

## [1.0.0] - 2026-05-29

### Added
- Initial release.
- BLE mouse + keyboard combo, paired as a generic "Logitech Combo" device.
- Three mouse movement styles with Bezier curves and ease-in-out timing across a virtual 600x600 field.
- Human-like typing at 60 to 80 WPM with per-keystroke variation and occasional thinking pauses.
- Word pool of 18 single words and 12 short phrases.
- Pause / resume via the onboard BOOT button.
- Status LED on the onboard LED.
- Serial logging that only runs when a USB host has the port open.
