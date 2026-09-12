# Bathroom Wall Panel — ESPHome + LVGL

Firmware for the ESP32-S3 touchscreen used as a smart-home wall panel by the
bathroom sink. Replaces the hand-written Arduino sketch with ESPHome + LVGL for
smoother rendering and native Home Assistant integration.

```
bathroom-panel.yaml                       ESPHome firmware (all three pages)
components/st77922_touch/                 touch driver for this panel (see below)
homeassistant/bathroom_panel_package.yaml HA helper entities the panel reads
secrets.yaml.example                      template for your secrets.yaml
```

> **Rotate the credentials in the Arduino sketch.** `AitherClock_ST77922_V2.ino`
> has the WiFi PSK and a long-lived Home Assistant token in plain text near the
> top. The token does not expire until 2036. Revoke it in HA (Profile →
> Security → Long-lived access tokens) and issue a new one, and keep that sketch
> out of any repo. Nothing in this repo contains either value — ESPHome reads
> both from `secrets.yaml`, which is gitignored.

## Hardware

Taken from the Arduino sketch and its `esp_panel_board_custom_conf.h`, not
guessed:

| | |
| --- | --- |
| Board | Elecrow DLE06235B, ESP32-S3 |
| Display | **ST77922 over QSPI**, 320×480 native, run landscape as **480×320** |
| QSPI pins | SCK 12, D0 11, D1 13, D2 14, D3 9, CS 10, 80 MHz |
| Touch | ST77922's own touch block, I²C **0x55**, SDA 38, SCL 39, INT 47 |
| Reset | GPIO 48, **shared between the LCD and the touch block**, active low |
| Backlight | GPIO 41, plain on/off enable (not PWM), active high |
| Timezone | `America/Chicago`, matching `CST6CDT` in the sketch |

Three consequences worth knowing about:

* **The panel is 320×480, not 240×320.** The original handoff note had the wrong
  part. Run landscape, that is 480×320, and the whole UI is laid out for it.
* **ESPHome has no ST77922 driver.** The display uses the generic `mipi_spi`
  platform in `model: CUSTOM` with the vendor init sequence ported across from
  `ESP_PANEL_BOARD_LCD_VENDOR_INIT_CMD()`. Commands ESPHome issues itself were
  stripped — COLMOD, MADCTL, INVON, SLPOUT, DISPON, and the per-draw
  CASET/RASET/RAMWR — leaving the 54 panel-specific gate, gamma and
  charge-pump writes.
* **The touch controller needed a driver.** It answers at 0x55 with 16-bit
  register addresses and is not an FT5x06, GT911 or CST816, so
  `components/st77922_touch/` is a small custom component built from the
  register map in the sketch. It reads the controller's own reported geometry
  at startup and feeds it into the touchscreen calibration, so reports that are
  not 1:1 with the panel do not land offset.

### The shared reset line

GPIO 48 resets the touch block and the LCD together, and the vendor BSP requires
it to be pulsed *before* the LCD is configured — doing it afterwards wipes the
panel init. Neither the display nor the touchscreen component owns it for that
reason. It is a plain GPIO output pulsed from `on_boot` at priority 700, which
lands between GPIO output setup (800) and display setup (600).

### Still unverified

**The speaker pins are guesses.** The Arduino sketch contains no audio code at
all, so there was nothing to extract. `pin_i2s_bclk` / `pin_i2s_lrclk` /
`pin_i2s_dout` default to GPIO 42 / 2 / 1, which avoid every pin the display and
touch use. Wrong values mean silence, not damage. To drop audio entirely, delete
the `i2s_audio`, `speaker` and `rtttl` blocks and the three `script_click` /
`script_chime` / `script_brush_done` scripts.

`psram_mode` is also unconfirmed — it defaults to `octal`, and `quad` is right
for most 8 MB-PSRAM S3 modules. A wrong value here is a boot loop.

## What it does

**Home** — clock, date, and a `S M T W T F S` weekday strip with today
highlighted. The middle zone swaps automatically:

* *Idle* — Caleb's room and outdoor temperature cards with trend arrows and a
  weather-condition icon, plus the joke/quote of the day.
* *Media* — album art, track title, artist, transport controls and a volume
  slider when Spotify is playing.

Two round buttons in the header: one forces idle ⇄ media (music icon in idle,
thermometer in media), one starts the tooth-brushing timer. Two large nav
buttons pinned to the bottom go to Pool and Fan.

**Pool** — read-only air and water temperatures, plus full-height Waterfall and
Bubblers tiles. The whole tile is the tap target.

**Fan** — one 190 px round toggle.

Pool and Fan return to Home after 30 s without a touch.

**Tooth-brushing timer** — a full-screen overlay with a 2:00 countdown and a
progress ring, a chime at every 30 s mark and a three-note flourish at the end.
Tap the overlay (or the header button) to cancel. Also exposed to HA as
`button.bathroom_panel_start_brushing_timer`.

## Entities

Three came straight out of the sketch and are already correct:

| Substitution | Value |
| --- | --- |
| `ent_pool_waterfall` | `switch.waterfall` |
| `ent_pool_bubblers` | `switch.bubblers` |
| `ent_fan` | `switch.master_fan` |

The rest are still placeholders, marked `# CHECK` in the substitutions block:
the two room temperatures, pool water temperature, and the Spotify player.

Two judgement calls to confirm:

* **The fan is a `switch`, not a `fan`.** The handoff note asked for
  `fan.master_bedroom_fan`, but the sketch only ever talks to
  `switch.master_fan`, so that is what the Fan page toggles. If you do have a
  `fan.` entity, change `ent_fan` and set `ent_fan_domain: fan` — the page calls
  `${ent_fan_domain}.toggle`.
* **Which fan?** `switch.master_fan` is the master bedroom, per the handoff
  note. The sketch also has `switch.calebs_fan`, which may be the one an
  11-year-old actually wants from his own bathroom. Change `ent_fan` and
  `ent_fan_title` if so.

## Setup

1. **Secrets.** `cp secrets.yaml.example secrets.yaml`, fill it in, and generate
   the API key with `openssl rand -base64 32`. `secrets.yaml` is gitignored.
2. **HA package.** Copy `homeassistant/bathroom_panel_package.yaml` into
   `<config>/packages/` and add to `configuration.yaml`:

   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```

   Replace each source entity marked `# <-- CHECK` — in particular the Ambient
   Weather rain-rate and solar-radiation entities, whose names vary by station
   — then restart HA.
3. **Entity IDs.** Fill in the remaining `ent_*` substitutions in
   `bathroom-panel.yaml`.
4. **Flash.** First time over USB, after that over the air. Run it from the
   repo root so the local `components/` directory resolves:

   ```bash
   pip install esphome
   esphome run bathroom-panel.yaml
   ```
5. **Allow actions.** In HA, Settings → Devices & Services → ESPHome →
   Bathroom Panel → Configure, and enable *Allow the device to perform Home
   Assistant actions*. Without it the toggles and media controls silently do
   nothing — the display will still update, which makes this an easy one to
   miss.

Requires ESPHome 2025.2 or newer (`min_version` enforces it). Validated with
`esphome config` against 2026.6.5.

## First-boot troubleshooting

Most of what can go wrong on a panel this custom shows up as one of these:

| Symptom | Try |
| --- | --- |
| Black screen, device on WiFi | Backlight. GPIO 41 is a binary light here; if the board actually wants PWM, swap `output: platform: gpio` for `ledc` and the light to `monochromatic`. |
| Garbled or torn image | `data_rate` — drop 80 MHz to 40 MHz. |
| Colours inverted | Remove `invert_colors: true`. The vendor sequence ended in INVON, which is why it is set. |
| Red and blue swapped | `color_order: bgr`. |
| Image rotated or mirrored | Flip `mirror_x` / `mirror_y` under `display:` **and** make the identical change under `touchscreen:`. |
| Colours subtly wrong everywhere | The vendor sequence set COLMOD (`0x3A`) to `0x01`; ESPHome sends the standard 16-bit `0x55`. If that is the problem, re-add `[0x3A, 0x01]` to the end of `init_sequence`. |
| Touch dead, `did not answer at 0x55` in the log | The shared reset pulse. The vendor driver wants 100 ms low / 100 ms high; the `on_boot` block uses 120/200. Lengthen it, or check that nothing else claims GPIO 48. |
| Taps land in the wrong place | The `transform:` under `touchscreen:` must match the one under `display:`. |

## How the mode switching works

| Media player state | Result |
| --- | --- |
| `playing` | media mode |
| `paused` | stays in media mode for `paused_grace_time` (3 min), then idle |
| `idle` / `off` / `unavailable` | idle mode |

Tapping the override button pins the opposite mode for `manual_hold_time`
(45 s), then automatic logic resumes. Because the override latches a global that
`script_apply_mode` checks first, an incoming HA state change can never snap the
screen back out from under a tap.

## Daily joke / quote

Fetched in HA, not on the panel — no TLS on the device, and sources can change
without reflashing. The automation runs at 05:30, on HA start, and any time the
cached value goes unknown. Odd days get a dad joke from `icanhazdadjoke.com`,
even days a quote from `zenquotes.io`. Both are key-free.

It tries up to three times and only overwrites `input_text.panel_daily_quote` on
success, so a failed fetch leaves yesterday's text on screen rather than
blanking it. Anything longer than 110 characters is rejected and retried, which
keeps the text inside the 290 px column. `sensor.panel_daily_quote` mirrors the
input_text and falls back to "Have a great day!" if it has never been set.

A JokeAPI URL with `safe-mode` is in the package as a commented alternative.

## Known limitations

* **Not compiled.** `esphome config` validates the whole configuration,
  including the custom component and the LVGL tree, but the sandbox this was
  built in cannot reach the PlatformIO package registry, so the C++ was never
  put through a compiler. Expect the first `esphome run` to be where real
  compile errors surface, most likely in the lambdas or `st77922_touch`.
* **Album art is a placeholder** — a rounded square with a music icon. The real
  thing needs `online_image` pointing at the media player's `entity_picture`,
  which is a relative path that has to be joined to your HA base URL at runtime
  via `online_image.set_url`. The transport controls and volume slider are
  fully wired.
* **Volume is debounced by 400 ms**, so dragging the slider sends one
  `volume_set` call at the end rather than a stream of them.
* **Trend thresholds** are ±1.0 °F/h, which is the spec's ±0.5 °F over 30 min
  expressed as a rate. Tune in the two template sensors if the arrows flicker.
* **Icon codepoints** were verified against the current MaterialDesign-Webfont
  `_variables.scss`. If a future MDI release renumbers one it renders as a blank
  box; the glyph table at the top of the YAML maps every codepoint to its name.
* **Speaker as a HA media_player** (TTS to the bathroom) is not implemented —
  that needs the `speaker` media player platform and pulls in a decoder stack.
  Tones use `rtttl`, so there are no audio files to host.
* **No chime on pool/fan toggles.** Straightforward to add: call `script_chime`
  from the `on_state` block of the relevant `binary_sensor`, gated on a
  boot-settled flag so the panel doesn't play a tune every time HA reconnects
  and pushes initial state.
* `sw_mute` silences the tap click and the timer chimes; it survives a reboot
  and is exposed to HA as *Panel sounds muted*.

## Still open

* Which Spotify account is linked in HA, and is it Premium? Transport and
  volume control both require Premium — without it the buttons will 403 and the
  panel has no way to show that.
* The I²S speaker pins, per **Still unverified** above.
* Whether the Fan page should point at the master bedroom or Caleb's room.
