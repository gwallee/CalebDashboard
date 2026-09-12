# Bathroom Wall Panel — ESPHome + LVGL

Firmware for a Hosyond ESP32-S3 3.5" touchscreen (240×320 IPS, capacitive
touch) used as a smart-home wall panel by the bathroom sink. Replaces the
hand-written Arduino sketch with ESPHome + LVGL for smoother rendering and
native Home Assistant integration.

```
bathroom-panel.yaml                       ESPHome firmware (all three pages)
homeassistant/bathroom_panel_package.yaml HA helper entities the panel reads
secrets.yaml.example                      template for your secrets.yaml
```

## What it does

**Home** — clock, date, and a `S M T W T F S` weekday strip with today
highlighted. The middle zone swaps automatically:

* *Idle* — Caleb's room and outdoor temperature cards with trend arrows and a
  weather-condition icon, plus the joke/quote of the day.
* *Media* — track title, artist, transport controls and a volume slider when
  Spotify is playing.

Two round buttons in the header: one forces idle ⇄ media (music icon in idle,
thermometer in media), one starts the tooth-brushing timer. Two large nav
buttons pinned to the bottom go to Pool and Fan.

**Pool** — read-only air and water temperatures, plus full-height Waterfall and
Bubblers tiles. The whole tile is the tap target.

**Fan** — one 140 px round toggle bound to a HA fan entity.

Pool and Fan return to Home after 30 s without a touch.

**Tooth-brushing timer** — a full-screen overlay with a 2:00 countdown and a
progress ring, a chime at every 30 s mark and a three-note flourish at the end.
Tap the overlay (or the header button) to cancel. Also exposed to HA as
`button.bathroom_panel_start_brushing_timer`.

## Hardware to confirm — read this before flashing

**The Arduino sketch was not supplied, so every pin and driver model in the
`substitutions:` block is a placeholder for this class of board.** Nothing below
was verified against real hardware. Pull the real values out of the sketch and
replace them:

| Substitution | What to look for in the sketch |
| --- | --- |
| `disp_model` | `ST7789V`, `ILI9341`, `ILI9488`… all live under one `ili9xxx` platform, so this is a one-line change |
| `disp_invert` | `tft.invertDisplay(true)` or `TFT_INVERSION_ON` |
| `pin_disp_*` | SCLK / MOSI / MISO / CS / DC / RST |
| `pin_backlight` | the pin driven by `analogWrite` / `ledcWrite` for brightness |
| `pin_touch_*` | touch I²C SDA/SCL plus INT and RST |
| touch platform | `gt911` is the default; change to `ft63x6` if it's an FT6236 |
| `pin_i2s_*` | BCLK / LRCLK / DOUT going to the amp |
| `flash_size`, `psram_mode` | `octal` vs `quad` — wrong here means a boot loop |

If the image is rotated or mirrored, change `mirror_x` / `mirror_y` under
`display:` **and make the identical change under `touchscreen:`**, or touches
will land in the wrong place.

Two more things the sketch may contradict:

* The panel is configured as 240×320 native with `swap_xy: true` for landscape.
  If the module is actually 320×480, change `dimensions` too.
* The speaker is assumed to be on an I²S amplifier. If it's a bare speaker on a
  DAC pin instead, replace the `i2s_audio` / `speaker` blocks with
  `output: esp8266_pwm`-style tone output and point `rtttl:` at it.

## Setup

1. **Secrets.** `cp secrets.yaml.example secrets.yaml`, fill it in, and generate
   the API key with `openssl rand -base64 32`. `secrets.yaml` is gitignored.
2. **Timezone.** Set `timezone:` at the top of `bathroom-panel.yaml` to your TZ
   database name (it ships as `America/Chicago`).
3. **HA package.** Copy `homeassistant/bathroom_panel_package.yaml` into
   `<config>/packages/` and add to `configuration.yaml`:

   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```

   Replace each source entity marked `# <-- CHECK` — in particular the Ambient
   Weather rain-rate and solar-radiation entities, whose names vary by station
   — then restart HA.
4. **Entity IDs.** Fill in the `ent_*` substitutions in `bathroom-panel.yaml`.
   The four the package creates (`..._trend`, `outdoor_condition`,
   `panel_daily_quote`) already match.
5. **Flash.** First time over USB, after that over the air:

   ```bash
   pip install esphome
   esphome run bathroom-panel.yaml
   ```
6. **Allow actions.** In HA, Settings → Devices & Services → ESPHome →
   Bathroom Panel → Configure, and enable *Allow the device to perform Home
   Assistant actions*. Without it the toggles and media controls silently do
   nothing — the display will still update, which makes this an easy one to
   miss.

Requires ESPHome 2025.2 or newer (`min_version` enforces it). Validated against
2026.6.5.

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
keeps the text inside the ~188 px column. `sensor.panel_daily_quote` mirrors the
input_text and falls back to "Have a great day!" if it has never been set.

A JokeAPI URL with `safe-mode` is in the package as a commented alternative.

## Known limitations

* **Icon codepoints** were verified against the current
  MaterialDesign-Webfont `_variables.scss`, so they are correct as of now. If a
  future MDI release renumbers one it will render as a blank box — the glyph
  table at the top of the YAML maps every codepoint to its icon name, and
  <https://pictogrammers.com/library/mdi/> has the current value.
* **Album art is a placeholder** — a rounded square with a music icon. The real
  thing needs `online_image` pointing at the media player's `entity_picture`,
  which is a relative path that has to be joined to your HA base URL at runtime
  via `online_image.set_url`. Sketched but not implemented; the transport
  controls and volume slider are fully wired.
* **Volume is debounced by 400 ms**, so dragging the slider sends one
  `volume_set` call at the end rather than a stream of them.
* **Trend thresholds** are ±1.0 °F/h, which is the spec's ±0.5 °F over 30 min
  expressed as a rate. Tune in the two template sensors if the arrows flicker.
* **Speaker as a HA media_player** (TTS to the bathroom) is not implemented —
  that needs the `speaker` media player platform and pulls in a decoder stack.
  Tones use `rtttl`, so there are no audio files to host.
* **No chime on pool/fan toggles.** Straightforward to add: call
  `script_chime` from the `on_state` block of the relevant `binary_sensor`,
  gated on a boot-settled flag so the panel doesn't play a tune every time HA
  reconnects and pushes initial state.
* `sw_mute` silences the tap click and the timer chimes; it survives a reboot
  and is exposed to HA as *Panel sounds muted*.

## Still open

* Which Spotify account is linked in HA, and is it Premium? Transport and
  volume control both require Premium — without it the buttons will 403 and the
  panel has no way to show that.
* Where the brushing-timer button should live. It is currently a second round
  header button beside the mode override; the alternative was a third tile.
* Display, touch and amp chips, per **Hardware to confirm** above.
