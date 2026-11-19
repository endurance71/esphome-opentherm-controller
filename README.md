# esphome-opentherm-controller

A DIY OpenTherm thermostat for gas boilers: a through-hole adapter on a perfboard, an ESP32 running ESPHome and Home Assistant on top.

It goes further than turning the boiler on and off. The ESP32 sets the **flow temperature** continuously from a **weather curve** with a slow **room correction**, so a condensing boiler runs long, at a low flame and with a cool return. That is where condensing boilers save gas.

> **Status: work in progress, not field-tested yet.**
> - Both ESPHome configs validate and compile (ESPHome 2026.10.2, ESP32, ESP-IDF 5.5.6).
> - The adapter layout passes the netlist check.
> - The first adapter is being built.
> - The first target boiler (ACV ILEA 18/25) is not yet confirmed to speak OpenTherm on its thermostat input.
>
> Results will be added under [`boilers/`](boilers/).

## What's inside

```
hardware/
  schematic.svg             adapter schematic (Ihor Melnyk's circuit + TX/RX LEDs + header)
  board-top.svg             perfboard layout, component side
  board-bottom.svg          perfboard layout, solder side (mirrored), wire traces
  board_layout.py           board model + netlist check (source of truth for the layout)
  bom.csv                   parts list with example TME part numbers
firmware/esphome/
  opentherm-diagnostic.yaml first test: reads everything, central heating starts OFF
  opentherm-weather-curve.yaml  target: weather curve + room P/I correction
  secrets.example.yaml
firmware/arduino/OpenThermScanner/
  OpenThermScanner.ino      deep scan of data IDs 1–255, safe write tests, TSP/FHB, web report
docs/
  build-guide.html          interactive build guide: schematic, board views, wiring, BOM
  control-plan.html         interactive control plan with a curve calculator
boilers/
  acv-ilea/                 notes for ACV ILEA (Atlantic Naia 2 platform)
```

The docs are in English. The ESPHome entity names and the comments in the firmware are still in Polish, e.g. „Grzanie CO” is central heating and „Grzanie CWU” is domestic hot water. Open the HTML pages from a local clone, or with any static HTML preview.

## Hardware

- **ESP32-WROOM DevKit**, powered from 5 V USB. `GPIO21` = RX (input from the adapter), `GPIO22` = TX (output to the adapter).
- **Adapter**: Ihor Melnyk's OpenTherm adapter circuit (MIT), plus two LEDs for TX/RX and a 1×4 pin header.
  - The boiler side is powered from the OpenTherm line itself and isolated from the ESP32 by two PC817 optocouplers.
  - The grounds of the two sides are **not** connected.
- **Board**: a **dot-type perfboard** with a separate pad on each hole. A stripboard will not work with this layout. It needs at least 19 × 29 holes at 2.54 mm; a common 7 × 9 cm board fits.
- If you change the layout, run `python3 hardware/board_layout.py`. It must print `NETLIST OK`.

## Firmware

1. **Diagnostic** (`opentherm-diagnostic.yaml`).
   - Exposes every sensor of ESPHome's `opentherm` component and logs the boiler's response type for each data ID.
   - Central heating starts **off** and domestic hot water stays on. Only the flow setpoint (ID 1) is written.
   - Do not leave it on the boiler permanently.
2. **Scan** (optional, Arduino).
   - Shows which data IDs the boiler answers and which writes it accepts.
   - Afterwards, delete the sensors your boiler does not support from the target config.
3. **Target** (`opentherm-weather-curve.yaml`). Every 30 s:

   ```
   curve      = target + slope × (target − outdoor) + offset
   correction = P × (target − room) + I        (slow integral, ±5 K)
   TSet       = clamp(curve + correction, min, max, 20–60 °C, boiler max CH setpoint)
   ```

   - The flow temperature rises at a limited rate and drops immediately.
   - Central heating stays enabled for the whole heating season. The ESP only turns it off above a summer threshold, or when the room is more than 1 K over target for 15 min.
   - Inputs come from Home Assistant: outdoor temperature, room temperature and the `temperature` attribute of a `climate` entity. Set them in `substitutions`.
   - The curve parameters are `number` entities that you can tune from Home Assistant.
   - **Without Home Assistant** the ESP does not reboot (`reboot_timeout: 0s`). After 10 minutes it keeps heating on the curve with the last known target. The outdoor temperature falls back to the boiler's sensor (ID 27), then to 0 °C.
   - The template switch „Grzanie CO” is meant as the `heater` of an HA `generic_thermostat` with a wide hysteresis, which acts only as a safety cut-off.

Copy `secrets.example.yaml` to `secrets.yaml` and fill it in. Both configs use the same device name, so the target config replaces the diagnostic one over OTA.

## Safety rules

- **This drives a gas boiler.** The boiler's own electronics keep handling flame, overheat and pressure safety. The ESP32 is only the thermostat (OpenTherm master).
- The firmware writes **only** the CH flow setpoint (ID 1), within 20–60 °C. The DHW setpoint (ID 56) and max CH setpoint (ID 57) are read-only. The ESPHome configs write nothing else. The scanner additionally sets max modulation (ID 14) to 100 % and writes ID 56/57 back with the values it has just read.
- Connect the adapter only to the boiler's low-voltage room-thermostat / OpenTherm terminals. **Never to 230 V.**
- Before the first test, write down the boiler's current settings and keep the old thermostat or relay wiring ready to put back.

## Adding a boiler

1. Create `boilers/<brand-model>/README.md` with the terminals, how to confirm OpenTherm on that boiler and the scan results.
2. Note any changes to the configs, such as unsupported sensors or a different `max` for the curve.

Pull requests with results from other boilers are welcome.

## Credits and licenses

- The adapter circuit is by Ihor Melnyk: [OpenTherm Adapter](https://ihormelnyk.com/opentherm_adapter) (MIT). The [xyzroe/OpenTherm-TTL-adapter](https://github.com/xyzroe/OpenTherm-TTL-adapter) schematic (CC BY-NC-SA 4.0) was used as a reading reference. No files from it are included here.
- [ESPHome OpenTherm component](https://esphome.io/components/opentherm/).
- The Arduino scanner uses [ihormelnyk/opentherm_library](https://github.com/ihormelnyk/opentherm_library) (MIT).
- Everything in this repository is released under the [MIT License](LICENSE).

Use at your own risk. If in doubt, ask a heating engineer.
