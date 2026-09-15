# Task: find the entity IDs for the bathroom wall panel, and set up its helpers

You have Home Assistant access over MCP. I do not — that is why this is being
handed to you.

I am building an ESPHome wall panel for a bathroom. Its firmware reads a
handful of Home Assistant entities, but five of them are still **guesses**
written by someone who could not see this HA instance. Your job is to find the
real ones and produce a filled-in configuration.

**Do not guess.** If you cannot find a confident match for something, say so
and leave it marked. A wrong entity ID here fails silently — the panel shows a
plausible-looking number or arrow that means nothing, which is far worse than
an obvious blank.

---

## Part 1 — Find these five entities

| # | What it is | Current guess | Used by the panel for |
|---|---|---|---|
| 1 | Caleb's bedroom temperature (°F) | `sensor.calebs_room_temperature` | indoor card + trend arrow |
| 2 | Outdoor temperature (°F), from an Ambient Weather station | `sensor.ambient_outdoor_temperature` | outdoor card + trend arrow + Pool page "Air" |
| 3 | Rain rate, Ambient Weather | `sensor.ambient_hourly_rain_rate` | weather icon |
| 4 | Solar radiation, Ambient Weather | `sensor.ambient_solar_radiation` | weather icon |
| 5 | Pool water temperature | `sensor.pool_water_temperature` | Pool page "Water" |

Plus one more, lower confidence:

| # | What it is | Current guess | Used for |
|---|---|---|---|
| 6 | Caleb's Spotify player | `media_player.spotify_caleb` | the panel's media mode |

Search the entity registry for `temperature`, `ambient`, `pool`, `solar`,
`rain`, and `media_player`. Ambient Weather naming varies between stations
and integration versions — rain rate in particular shows up as
`..._hourly_rain`, `..._hourly_rain_rate`, `..._rain_rate` or `..._rainrate`,
so check what actually exists rather than assuming.

**Also check and report these, because they change the configuration below:**

- **Units.** Are the temperatures in °F or °C? The panel formats them as
  whole degrees Fahrenheit. If HA is serving Celsius, say so.
- **Solar radiation unit.** The condition template assumes **W/m²** and uses
  250 as the sunny/cloudy split. If the sensor reports lux or something else,
  that threshold is meaningless — tell me the unit and a typical midday value.
- **Rain rate unit.** The template only tests `> 0`, so units barely matter,
  but confirm it is a *rate* and not a cumulative daily total. A daily total
  would leave the icon stuck on "rain" for the rest of every rainy day.
- **Spotify Premium.** If you can tell whether the linked account is Premium,
  say so. Transport and volume control require it, and the panel has no way to
  show that a call failed.

---

## Part 2 — Produce the filled-in package

Take the YAML at the end of this file and replace the four entity IDs marked
`CHECK` with the real ones you found — #1, #2, #3 and #4 from the table above.
Change nothing else unless Part 1 turned up a reason to (wrong units, a
different solar threshold).

**#5 (pool water) and #6 (Spotify) are not in this package.** The panel's
firmware reads those directly, so they are not yours to install — just report
them and I will put them in the firmware's substitutions.

Leave the entity IDs the package *creates* alone — `..._derivative`,
`..._trend`, `sensor.outdoor_condition`, `sensor.panel_daily_quote` and
`input_text.panel_daily_quote`. The panel's firmware already expects those
exact names.

## Part 3 — Install it, if you can

Where it goes:

```
<HA config>/packages/bathroom_panel_package.yaml
```

and `configuration.yaml` needs, once:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Then check the configuration and restart HA.

If your MCP access is read-only — which is likely, most HA MCP servers expose
states and services but not the filesystem — **do not fake it.** Say clearly
that you could not write the file, and hand back the finished YAML for me to
paste in myself. That is a perfectly good outcome; Part 1 is the part I cannot
do without you.

## Part 4 — Report back

Give me, in this order:

1. A table of the six entity IDs: what I guessed → what is actually there.
   Flag any you could not find.
2. The answers to the unit/Premium questions above.
3. The complete filled-in package YAML.
4. Whether you installed it or not.

I will take that back to the repo where the panel firmware lives and update
the matching substitutions there.

---

## The package to fill in

What each piece does:

- **Two `derivative` sensors** — rate of change of each temperature over a
  30-minute window, in °F per hour.
- **Two template sensors** — turn that rate into the three-state string
  `rising` / `steady` / `falling` that drives the panel's trend arrows. The
  ±1.0 °F/h threshold is a ±0.5 °F-over-30-min deadband expressed as a rate.
- **One template sensor** — collapses rain rate, solar radiation and `sun.sun`
  into `sunny` / `cloudy` / `rain` / `night`, which picks the weather icon.
- **Quote plumbing** — an `input_text`, two `rest_command`s and a daily
  automation that fetches a kid-safe dad joke on odd days and a quote on even
  ones. Deliberately in HA rather than on the ESP32: no TLS on the device, and
  sources can change without reflashing.

The `has_value()` availability guards are deliberate. Without them a missing
source floats to 0, which reads as a confident "steady" arrow or a "cloudy"
icon. Keep them.

```yaml
# =============================================================================
# Home Assistant package for the bathroom wall panel
#
# Install:
#   1. mkdir -p <config>/packages
#   2. copy this file to <config>/packages/bathroom_panel_package.yaml
#   3. in configuration.yaml:
#
#        homeassistant:
#          packages: !include_dir_named packages
#
#   4. Replace every source entity id marked  # <-- CHECK  with your real one,
#      then Developer Tools -> YAML -> Restart.
#
# Provides:
#   sensor.calebs_room_temperature_trend      rising | steady | falling
#   sensor.ambient_outdoor_temperature_trend  rising | steady | falling
#   sensor.outdoor_condition                  sunny | cloudy | rain | night
#   sensor.panel_daily_quote                  today's joke or quote
# =============================================================================

# -----------------------------------------------------------------------------
# Temperature rate of change.
#
# unit_time: h means the derivative is °F per hour. The panel spec asked for a
# ±0.5 °F deadband over a ~30 min window, which is ±1.0 °F/h — that is the
# threshold used in the template sensors below.
# -----------------------------------------------------------------------------
sensor:
  - platform: derivative
    source: sensor.calebs_room_temperature            # <-- CHECK
    name: "Caleb's room temperature derivative"
    unique_id: calebs_room_temperature_derivative
    unit_time: h
    time_window: "00:30:00"
    round: 2

  - platform: derivative
    source: sensor.ambient_outdoor_temperature        # <-- CHECK
    name: "Outdoor temperature derivative"
    unique_id: ambient_outdoor_temperature_derivative
    unit_time: h
    time_window: "00:30:00"
    round: 2

template:
  - sensor:
      # --- trend arrows -----------------------------------------------------
      - name: "Caleb's room temperature trend"
        unique_id: calebs_room_temperature_trend
        state: >-
          {% set d = states('sensor.calebs_room_temperature_derivative') | float(0) -%}
          {{ 'rising' if d > 1.0 else ('falling' if d < -1.0 else 'steady') }}
        # Without this a missing source sensor floats to 0 and reads as a
        # confident "steady" -- an arrow that looks right and means nothing.
        availability: "{{ has_value('sensor.calebs_room_temperature_derivative') }}"

      - name: "Ambient outdoor temperature trend"
        unique_id: ambient_outdoor_temperature_trend
        state: >-
          {% set d = states('sensor.ambient_outdoor_temperature_derivative') | float(0) -%}
          {{ 'rising' if d > 1.0 else ('falling' if d < -1.0 else 'steady') }}
        # Without this a missing source sensor floats to 0 and reads as a
        # confident "steady" -- an arrow that looks right and means nothing.
        availability: "{{ has_value('sensor.ambient_outdoor_temperature_derivative') }}"

      # --- weather icon driver ----------------------------------------------
      # Rain wins, then darkness, then "is the sun actually hitting the
      # station". 250 W/m2 is a reasonable sunny/overcast split; raise it if
      # hazy days read as sunny.
      - name: "Outdoor condition"
        unique_id: outdoor_condition
        state: >-
          {% set rain  = states('sensor.ambient_hourly_rain_rate') | float(0) -%}   {# <-- CHECK #}
          {% set solar = states('sensor.ambient_solar_radiation') | float(0) -%}    {# <-- CHECK #}
          {{ 'rain'  if rain > 0
             else ('night'  if is_state('sun.sun', 'below_horizon')
             else ('sunny'  if solar > 250
             else 'cloudy')) }}
        # Same reasoning: no station data should read as unavailable, not as
        # a cloudy day.
        availability: >-
          {{ has_value('sensor.ambient_hourly_rain_rate')
             and has_value('sensor.ambient_solar_radiation') }}

      # --- what the panel actually reads ------------------------------------
      # Backed by the input_text below so the last good string survives a
      # Home Assistant restart and the screen is never blank.
      - name: "Panel daily quote"
        unique_id: panel_daily_quote
        state: >-
          {% set v = states('input_text.panel_daily_quote') -%}
          {{ v if v not in ['unknown', 'unavailable', ''] else 'Have a great day!' }}

# -----------------------------------------------------------------------------
# Daily joke / quote
# -----------------------------------------------------------------------------
input_text:
  panel_daily_quote:
    name: Panel daily quote
    max: 255
    icon: mdi:comment-quote

rest_command:
  # Family-safe by nature, no API key.
  panel_fetch_joke:
    url: "https://icanhazdadjoke.com/"
    method: GET
    headers:
      Accept: "application/json"
      User-Agent: "HomeAssistant bathroom panel (https://www.home-assistant.io)"
    timeout: 15

  # Alternative with an explicit family filter, if dad jokes wear thin:
  #   url: "https://v2.jokeapi.dev/joke/Any?safe-mode&type=single"
  #   -> response is {"joke": "..."}

  panel_fetch_quote:
    url: "https://zenquotes.io/api/random"
    method: GET
    timeout: 15

automation:
  - id: panel_daily_quote_refresh
    alias: "Panel: refresh daily joke/quote"
    description: >-
      Odd days get a dad joke, even days an inspirational quote. Tries up to
      three times and only overwrites the cached string on success, so a failed
      fetch leaves yesterday's text on screen.
    mode: single
    max_exceeded: silent
    trigger:
      - platform: time
        at: "05:30:00"
      - platform: homeassistant
        event: start
      - platform: state
        entity_id: input_text.panel_daily_quote
        to: "unknown"
    action:
      - variables:
          want_joke: "{{ now().day % 2 == 1 }}"
          max_len: 110
      - repeat:
          count: 3
          sequence:
            - if:
                - condition: template
                  value_template: "{{ want_joke }}"
              then:
                - action: rest_command.panel_fetch_joke
                  response_variable: resp
                - variables:
                    text: >-
                      {{ (resp.content.joke | default('', true)) if resp.status == 200 else '' }}
                - if:
                    - condition: template
                      value_template: "{{ text | length > 0 and text | length <= max_len }}"
                  then:
                    - action: input_text.set_value
                      target:
                        entity_id: input_text.panel_daily_quote
                      data:
                        value: "{{ text }}"
                    - stop: "stored a joke"
              else:
                - action: rest_command.panel_fetch_quote
                  response_variable: resp
                - variables:
                    text: >-
                      {{ (resp.content[0].q ~ ' — ' ~ resp.content[0].a)
                         if resp.status == 200 else '' }}
                - if:
                    - condition: template
                      value_template: "{{ text | length > 0 and text | length <= max_len }}"
                  then:
                    - action: input_text.set_value
                      target:
                        entity_id: input_text.panel_daily_quote
                      data:
                        value: "{{ text }}"
                    - stop: "stored a quote"
            - delay: "00:00:10"
```
