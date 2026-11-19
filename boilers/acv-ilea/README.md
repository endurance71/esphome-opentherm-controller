# ACV ILEA (Atlantic Naia 2 platform)

Target unit: **ACV ILEA 18/25**, a combi (CH + DHW) condensing gas boiler. It modulates 4.0–18.5 kW for CH and gives 24 kW for DHW. Built in 2021.

**Status:** OpenTherm on this boiler is **not confirmed yet**. The adapter is being built, and these notes will be updated with the scan results.

## What we know from the manuals

| Topic | Finding | Source |
|---|---|---|
| Platform | ILEA = Atlantic **Naia 2**. The manuals are the same, with the same "info" menu numbering and even the same typo. | ACV ILEA FR/PL vs Atlantic Naia 2 Micro manuals |
| OpenTherm | Navilink 105/125/128 room thermostats talk **OpenTherm** to the boiler on the zone thermostat input (Z1/Z2). The ILEA manual describes working with Navilink 105/128 but never says "OpenTherm". **Confirm by testing.** | Navilink 125/128 manual, ILEA PL manual |
| Precedent | [gduteil/OpenThermGateway](https://github.com/gduteil/OpenThermGateway) (ESPHome) reportedly runs stably on an Atlantic Naema 2 Micro 25 with Navilink 128. | project README |
| PASS connector | For Navipass IO, which uses **Modbus** on Naia/Naema (A/B/0V/+24V). No public register map, so this stays plan B. | Navipass IO manual 073329 |
| Cloud (Cozytouch/Overkiz) | Almost nothing for Naia 2 in Home Assistant (only DHW and room temperature). Rejected. | home-assistant/core #89537 |

## Terminals (low-voltage block)

| Terminals | Function |
|---|---|
| 1–2 | Zone 1 room thermostat: dry contact **or** Navilink bus (OpenTherm). **The adapter goes here.** |
| 3–4 | Zone 2 |
| 5–6 | Modem |
| 7–8 | Outdoor sensor, 10 kΩ |
| 9–10 | Underfloor heating safety |

These are safety extra-low-voltage terminals. Never connect anything to the 230 V terminals.

## "Info" menu (useful while testing)

| Item | Meaning |
|---|---|
| 1 | Pressure |
| 3 | Outdoor temperature |
| 4 | Flow temperature |
| 5 | Flow setpoint |
| 6 | Return temperature |
| 10 | Boiler state |
| 11 | Modulation |
| 12 | Fan |
| 13 | Ionisation |
| 14 | Burner state |
| 15 | Flue gas |
| 21–23 | Zone 1 |
| 30–34 | DHW |
| 40–43 | Zone 2 |
| 50–52 | Last fault |
| 60–67 | Energy |

**Info 22 (zone 1)** should show state **4 ("Navilink 105/128")** once the boiler sees an OpenTherm thermostat.

## Test plan

1. Solder the adapter on a dot perfboard and check the netlist (`python3 hardware/board_layout.py`).
2. With a multimeter, check for no continuity between any boiler-side point and the ESP32 GND. Measure the resistors before soldering them.
3. Measure the DC voltage on the free Z1 terminals with the boiler on. Roughly 15–24 V is a good sign but not proof.
4. Flash `opentherm-diagnostic.yaml`, connect the adapter to terminals 1–2 with the boiler off, then turn the boiler on. The LEDs should blink.
5. Record which entities get values, and save the logs with the response types (`READ_ACK`, `DATA_INVALID`, `UNKNOWN_DATAID`).
6. Control test: turn "Central heating" on and set "CH flow setpoint" to 40 °C. Expected: flame on, modulation > 0, flow temperature rising.
7. Failure test: disconnect the ESP32 for a few minutes and see what the boiler does (fallback mode or its own control).
8. Afterwards, either restore the old on/off control or switch to `opentherm-weather-curve.yaml`.

Test during the day, in mild weather, and keep it short. Write down the boiler's CH and DHW settings first.

**If the boiler doesn't answer (only timeouts in the logs)**, try these one at a time:
1. swap `in_pin` / `out_pin`;
2. set `inverted: true` on the pins;
3. set `sync_mode: true`.

If it still doesn't answer, this firmware has no OpenTherm (parameter 9 = software version), so go back to the on/off contact.

## Tuning notes for a condensing setup

- **Goal:** a return temperature below about 45 °C for most of the heating time, and as few burner starts as possible.
- **Start the curve** where the boiler runs today, at a similar flow temperature at +2…+7 °C outside. Then lower it step by step while the house stays warm.
- **Night setbacks:** keep them small (1–1.5 K). A deep setback means hot water in the morning and no condensing.
- **Hydraulics:**
  - open all radiators fully;
  - use a lower pump speed if available, for a bigger flow–return difference;
  - check the anti-cycling parameter in the boiler menu.
