# Changelog

All notable changes to this project are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
- The `FIELD` constant is replaced by the `fieldSize` variable with `DEFAULT_FIELD` as its starting value. Changing it at runtime clamps the virtual cursor position into the new bounds.
- Mode selection in the main loop respects the `mouseEnabled` and `keyboardEnabled` flags. If only one is on, only that action type runs.

### Notes
- Runtime changes are not persisted. After a power cycle, settings revert to the sketch defaults. For permanent changes, edit the `DEFAULT_*` constants and reflash. Optional NVS-based persistence could come in a later release.
- Disabling both modes at once is prevented. The wiggler needs at least one action type enabled.

## [1.2.0] - 2026-05-29

### Added
- Serial command interface for runtime control without touching the BOOT button. Commands: `pause`, `resume` (alias `start`), `toggle`, `status`, `now`, `help` (alias `?`).
- `now` command forces the next action immediately, useful for testing the setup.
- `status` command prints wiggler state, BLE connection, active layout, and time until the next action.
- Boot message now mentions the `help` command so it's discoverable.

### Changed
- Serial commands and button input are evaluated at every wait point inside the mouse and keyboard loops, so both react mid-action instead of waiting for the current step to finish.
- Internal `forceAction` flag added to cleanly trigger the next action on demand.

## [1.1.0] - 2026-05-29

### Added
- QWERTZ keyboard layout support via a single `QWERTZ` constant near the top of the sketch. Set it to `true` for German keyboards, `false` for QWERTY.
- Layout is logged to serial on boot, so it's visible which mode is active.

### Changed
- Default value of `QWERTZ` is `true`. If your host uses QWERTY, flip it before flashing.

### Notes
- No library updates required. Pull, edit the constant if needed, reflash.

## [1.0.0] - 2026-05-29

### Added
- Initial release.
- BLE mouse + keyboard combo, paired as a generic "Logitech Combo" device.
- Three mouse movement styles with Bezier curves and ease-in-out timing across a virtual 600x600 field.
- Human-like typing at 60 to 80 WPM, with per-keystroke variation and occasional thinking pauses.
- Word pool of 18 single words and 12 short phrases.
- Pause / resume via the onboard BOOT button (GPIO 9), interrupt-driven so it reacts mid-action.
- Status LED on the onboard LED (GPIO 8): heartbeat when active, solid when paused, fast blink while waiting for BLE.
- Serial logging that only runs when a USB host has the port open. On a power bank the wiggler stays silent.
