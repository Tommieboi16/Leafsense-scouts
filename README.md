# LeafSense Scouts

Two STM32 microcontrollers form a miniature crop scouting network over Zigbee. A **Field Scout** reports disease sightings; a **Base Scout** verifies them, maintains a persistent field map, and acknowledges. Built for the 6ENT1180 Robot Communications module and **graded 91.5 / 100**.

The interesting part is not that the radios talk. It is that the link is built to survive someone actively trying to abuse it.

## The problem

A naive sensor network broadcasts plaintext. Anyone with a matching radio can inject fake disease reports, or capture a genuine frame and replay it a hundred times to poison the map. Agricultural telemetry that can be spoofed is worse than no telemetry, because decisions get made on it.

## Design

Every event is a **four byte payload** wrapped in an application layer security envelope:

| Byte | Meaning |
|------|---------|
| `0xE0` | Event marker |
| `zone` | Field zone id |
| `class` | Disease class, reusing the PlantVillage taxonomy |
| `severity` | Observed severity |

Defence layers, in order of what they stop:

- **AES-128 encryption** so payloads are not readable in the air
- **A nonce per frame** so a captured frame cannot be replayed, the Base Scout rejects a nonce it has already seen
- **A hardware UID allow list** so frames from an unknown device are dropped rather than processed
- **An explicit auth-fail acknowledgement**, so a spoofed sender learns only that it failed, and the operator can see the attempt

Failed frames are answered, not silently dropped. Silence is indistinguishable from a flat battery; an explicit `auth fail` is diagnosable.

## Persistence

The Base Scout writes to an I2C EEPROM laid out as **one page per zone**, holding cumulative counts plus a ring buffer of recent events. The ring buffer means a zone under repeated attack does not overflow into its neighbour's page, and the whole map can be dumped over serial for audit.

## Layout

| File | Role |
|------|------|
| `main.c` / `main.h` | Application loop and state |
| `Zigbee.c` / `Zigbee.h` | Frame construction, checksums, UART interrupt receive, secure send and verify |
| `aes.c` / `aes.h` | AES-128 implementation |
| `EEPROM.c` / `EEPROM.h` | I2C EEPROM driver |
| `FieldMap.c` / `FieldMap.h` | Zone paging, cumulative stats, ring buffer |
| `LeafSense.c` / `LeafSense.h` | Disease class definitions and event encoding |
| `Led.c` / `Led.h` | Status indication |

## Hardware

- 2 x ST Nucleo-F411RE
- 2 x Digi XBee S2C, Zigbee 802.15.4
- I2C EEPROM for the field map

## Notes

Receive runs off a UART interrupt with a single byte ISR buffer and a byte-at-a-time frame parser, so the main loop is never blocked waiting on the radio.

The disease codes deliberately match the taxonomy used by [AgriSense AI](https://agrisense-scann.netlify.app/), so a scout and the phone app describe the same finding the same way.
