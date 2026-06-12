# Voltas 183V DZU2 IR Protocol -- Complete Analysis

## Signal Structure

48-bit NEC-extended variant. Two identical frames per transmission.

```
|-- Header --|----------- Frame 1 (48 bits) ----------|-- Gap --|---- Frame 2 (repeat) ----|
  4450us mark                                          ~5200us
  4450us space
```

Bit encoding: mark ~530us, zero space ~530us, one space ~1650us.

## Byte Map

```
Byte:  B0    B1    B2       B3       B4         B5
Role:  addr  addr  command  ~B2      data       ~B4
Fixed: 0xB2  0x4D  varies   ~B2      varies     ~B4 (usually)
```

- B0/B1 = device address (see below)
- B3 = bitwise NOT of B2 (always)
- B5 = bitwise NOT of B4 (except timer, sleep off, eco commands)

### Device Addresses

| Address  | Used for                                      |
|----------|-----------------------------------------------|
| `B2 4D`  | Standard: modes, temp, fan, swing, power, sleep, timers |
| `B5 4A`  | Special: turbo, eco, display, dehumidify       |
| `BA 45`  | Follow me                                     |

---

## B2 -- Command Byte

### Fan Speed (bits 7-6)

| Fan    | B2[7:6] | B2 value (cool, no timer) |
|--------|---------|---------------------------|
| High   | `00`    | `0x3F`                    |
| Medium | `01`    | `0x5F`                    |
| Low    | `10`    | `0x9F`                    |
| Auto   | `10`    | `0xBF`                    |

Fan bits detail:
```
High:  00_111111  = 0x3F
Med:   01_011111  = 0x5F
Low:   10_011111  = 0x9F
Auto:  10_111111  = 0xBF
```

### Special B2 values

| Function     | B2     | Notes                          |
|--------------|--------|--------------------------------|
| Power off    | `0x7B` | B4=0xE0                        |
| Swing on     | `0x6B` | B4=0xE0                        |
| Swing off    | `0x0F` | B4=0xE0                        |
| Dry mode     | `0x1F` | Fan auto forced                |
| Sleep on     | `0xE0` | B4=0x03                        |
| Sleep off    | `0xAB` | B4=temp|0x01, B5=0xFF          |
| Timer off    | varies | See timer encoding below       |
| Timer on     | normal | Timer data in B4/B5            |

---

## B4 -- Data Byte

### Temperature (upper nibble, bits 7-4)

Gray code encoding: `nibble = gray_encode(temp - 17)`
(with minor deviation at 27-28C)

| Temp | Nibble | Binary | B4 (cool mode) |
|------|--------|--------|----------------|
| 17C  | 0x0    | 0000   | 0x00           |
| 18C  | 0x1    | 0001   | 0x10           |
| 19C  | 0x3    | 0011   | 0x30           |
| 20C  | 0x2    | 0010   | 0x20           |
| 21C  | 0x6    | 0110   | 0x60           |
| 22C  | 0x7    | 0111   | 0x70           |
| 23C  | 0x5    | 0101   | 0x50           |
| 24C  | 0x4    | 0100   | 0x40           |
| 25C  | 0xC    | 1100   | 0xC0           |
| 26C  | 0xD    | 1101   | 0xD0           |
| 27C  | 0x9    | 1001   | 0x90           |
| 28C  | 0x8    | 1000   | 0x80           |
| 29C  | 0xA    | 1010   | 0xA0           |
| 30C  | 0xB    | 1011   | 0xB0           |

Each consecutive temperature changes exactly 1 bit (Gray code property).

### Mode (lower nibble, bits 3-0)

| Mode     | B4[3:0] | Example B4 @27C |
|----------|---------|-----------------|
| Cool     | `0000`  | `0x90`          |
| Dry      | `0100`  | `0x94`          |
| Heat     | `1100`  | `0x9C`          |
| Fan Only | `0100`  | `0xE4`          |
| Off      | `0000`  | `0xE0`          |

---

## Special Commands (Address B5 4A)

These use a different device address and B2=`0xF5`.

| Command            | B4     | B5     | B5=~B4? |
|--------------------|--------|--------|---------|
| Turbo toggle       | `0xA2` | `0x5D` | yes     |
| Dehumidify+swing   | `0xAA` | `0x55` | yes     |
| Display on/off     | `0xA5` | `0x5A` | yes     |
| Display toggle     | `0xA5` | `0x2D` | no      |
| Eco on             | `0x82` | `0x90` | no      |
| Eco off            | `0x83` | `0x90` | no      |

---

## Follow Me (Address BA 45)

Uses a third device address. The remote's built-in temperature sensor
provides readings to the AC unit for more accurate cooling.

| Command         | B0 B1  | B2   | B4         | B5   |
|-----------------|--------|------|------------|------|
| Follow me on    | `BA 45`| `5F` | temp\|0x02 | ~B4  |
| Follow me off   | `B2 4D`| `0xAB`| temp\|0x01| 0xFF |

Follow me off uses the same code as sleep off.

---

## Sleep Mode (Standard Address)

Sleep mode sets the fan to auto and gradually adjusts temperature overnight.

| Command    | B2     | B4     | B5   |
|------------|--------|--------|------|
| Sleep on   | `0xE0` | `0x03` | ~B4  |
| Sleep off  | `0xAB` | temp\|0x01 | 0xFF |

---

## Timer Encoding

Timer commands use non-standard B5 values (B5 != ~B4).

### Timer OFF (AC turns off after N hours)

Duration range: 0.5h to 10h in 0.5h steps, then 11h to 24h in 1h steps.

```
half_hours = hours * 2        (1-48)
page       = (half_hours - 1) / 16    (0, 1, or 2)
index      = (half_hours - 1) % 16    (0-15)

B2 = 0x21 + index * 2
B4 = (temp_nibble << 4) | page
B5 = 0xFF
```

Examples:

| Hours | half_hrs | page | index | B2   | B4 @27C |
|-------|----------|------|-------|------|---------|
| 0.5   | 1        | 0    | 0     | 0x21 | 0x90    |
| 1.0   | 2        | 0    | 1     | 0x23 | 0x90    |
| 8.0   | 16       | 0    | 15    | 0x3F | 0x90    |
| 8.5   | 17       | 1    | 0     | 0x21 | 0x91    |
| 16.0  | 32       | 1    | 15    | 0x3F | 0x91    |
| 17.0  | 34       | 2    | 1     | 0x23 | 0x92    |
| 24.0  | 48       | 2    | 15    | 0x3F | 0x92    |

### Timer ON (AC turns on after N hours)

Duration range: 0.5h to 24h in 0.5h steps.

```
B2 = normal fan byte
B4 = (temp_nibble << 4) | 0x03
B5 = 0x81 + (half_hours - 1) * 2
```

### Cancel Timer

Send a normal state command (no timer encoding). This cancels any active timer.

---

## Constructing New Commands

To synthesize any standard command:

```
B0 = 0xB2
B1 = 0x4D
B2 = fan_speed_byte (see fan table)
B3 = ~B2 & 0xFF
B4 = (temp_nibble << 4) | mode_nibble
B5 = ~B4 & 0xFF
```

### Temperature lookup table (temp -> nibble):
```python
TEMP_NIBBLE = {
    17: 0x0, 18: 0x1, 19: 0x3, 20: 0x2,
    21: 0x6, 22: 0x7, 23: 0x5, 24: 0x4,
    25: 0xC, 26: 0xD, 27: 0x9, 28: 0x8,
    29: 0xA, 30: 0xB,
}
```

### Fan speed lookup:
```python
FAN_BYTE = {
    "high": 0x3F,
    "medium": 0x5F,
    "low": 0x9F,
    "auto": 0xBF,
}
```

### Mode lookup:
```python
MODE_NIBBLE = {
    "cool": 0x0,
    "dry": 0x4,
    "heat": 0xC,
    "fan": 0x4,  # with temp nibble = 0xE
}
```
