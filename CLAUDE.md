# CalebDashboard — working notes

ESPHome + LVGL firmware for a bathroom wall panel, replacing a hand-written
Arduino sketch. `README.md` has the full picture; this file is the short
version plus the things that are easy to get wrong.

## Status: never compiled

`esphome config` passes, so the YAML schema and the LVGL widget tree are
sound. **The C++ has never been through a compiler** — it was authored in a
sandbox that could not reach the PlatformIO package registry. That means
roughly 250 lines of lambdas and the whole of `components/st77922_touch/`
are unverified. Expect the first `esphome run` to surface real errors. That
is the current job.

## Hardware, established from the Arduino sketch

Not guessed — taken from `esp_panel_board_custom_conf.h`:

| | |
| --- | --- |
| Board | Elecrow DLE06235B, ESP32-S3 |
| Display | ST77922 over **QSPI**, 320x480 native, run landscape as **480x320** |
| QSPI | SCK 12, D0 11, D1 13, D2 14, D3 9, CS 10, 80 MHz |
| Touch | ST77922's own block, I2C **0x55**, 16-bit registers, SDA 38, SCL 39, INT 47 |
| Reset | GPIO 48, **shared by LCD and touch**, active low |
| Backlight | GPIO 41, on/off enable, not PWM |

Still unverified: the I2S speaker pins (GPIO 42/2/1) are guesses — the sketch
has no audio code at all. `psram_mode: octal` is also unconfirmed; `quad` is
right for most 8MB-PSRAM S3 modules and a wrong value is a boot loop.

## Things that will bite

* **ESPHome has no ST77922 driver.** The display is `mipi_spi` in
  `model: CUSTOM` with the vendor init sequence ported in. Commands ESPHome
  issues itself were stripped — COLMOD, MADCTL, INVON, SLPOUT, DISPON and the
  per-draw CASET/RASET/RAMWR. Do not re-add them without reading why.
* **Explicit `dimensions` are not swapped for `swap_xy`.** Landscape is
  declared `480x320`, not the native portrait size. This is counterintuitive
  and easy to "fix" wrongly.
* **GPIO 48 must be pulsed before the LCD is configured**, or the panel init
  is wiped. Neither component owns it; it is an `output` pulsed from `on_boot`
  at priority 700, between GPIO setup (800) and display setup (600).
* **The `transform:` under `touchscreen:` must match the one under
  `display:`.** Change one, change both, or taps land in the wrong place.
* **`external_components` has `refresh: 1d`.** While iterating on the touch
  driver set it to `0s`, or builds silently use a stale clone. When working
  from a local checkout, prefer the `type: local` form (both are in the file,
  one commented).
* **`logger: level: DEBUG` is deliberate.** `dump_config()` logs at CONFIG
  level, which INFO hides. Keep DEBUG until the panel is behaving.

## Bring-up

```bash
esphome config bathroom-panel.yaml   # seconds, no compile
esphome run    bathroom-panel.yaml   # compile + flash + logs
esphome logs   bathroom-panel.yaml   # once it is on WiFi
esphome clean  bathroom-panel.yaml   # when a build goes strange
```

First flash must be over USB; everything after is OTA. First compile is slow
(esp-idf + LVGL), later ones incremental.

The line to look for at boot:

```
ST77922 Touchscreen:
  Firmware version: N
  Max touch points: N
  Raw range: 320 x 480
```

That means the driver found the controller. `Controller did not answer at
0x55` means the shared reset pulse in `on_boot` needs lengthening.

`README.md` has a troubleshooting table for display symptoms — inverted,
mirrored, garbled, wrong colours — each a one-line change.

## Decisions already made, do not re-litigate

* **Landscape 480x320.** Confirmed by the owner, even though the Arduino
  sketch runs the panel portrait.
* **Fan page points at `switch.calebs_fan`**, not the master bedroom the
  original handoff note named. Confirmed by the owner. It is a switch, not a
  fan entity; the page calls `${ent_fan_domain}.toggle` so moving to a `fan.`
  entity is a two-substitution change.
* **The daily joke/quote is fetched in Home Assistant, not on the panel.**
  Deliberate: no TLS on the ESP32 and sources can change without reflashing.

## Still open

* Entity IDs marked `# CHECK` in the substitutions block: both room
  temperatures, pool water, pool air, and the Spotify player.
* Whether the Spotify account is Premium — transport and volume control
  require it, and the panel cannot show that it failed.
* Album art is a placeholder; the real thing needs `online_image` plus
  `online_image.set_url` to join `entity_picture` to the HA base URL.

## Repo

Work happens on `claude/test-sgpvp8`. `homeassistant/bathroom_panel_package.yaml`
is Home Assistant's side — it goes in `<HA config>/packages/` and ESPHome
cannot fetch it. `secrets.yaml` is gitignored.
