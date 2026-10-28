# ESPHome KlikAanKlikUit (KAKU) External Component

An ESPHome external component implementing the **KlikAanKlikUit (KAKU) / CoCo** 433.92 MHz RF protocol for transmitting and receiving commands via ESPHome's standard `remote_transmitter` and `remote_receiver` components.

---

## Installation via Git in ESPHome YAML

You can reference this repository directly using `external_components`:

```yaml
external_components:
  - source: github://quinn-v/kaku # or your git repository URL
    components: [ kaku ]
    refresh: 1d

# Activate component to register kaku protocol with remote_base
kaku:
```

Or for local development:
```yaml
external_components:
  - source:
      type: local
      path: components
    components: [ kaku ]

kaku:
```

---

## Hardware Configuration (Transmitter & Receiver)

```yaml
# 433 MHz RF Transmitter
remote_transmitter:
  id: rf_transmitter
  pin: GPIO18
  carrier_duty_percent: 100%

# 433 MHz RF Receiver
remote_receiver:
  id: rf_receiver
  pin: GPIO19
  dump:
    - kaku
```

---

## Usage Examples

### 1. Switch Using ESPHome Template Switch

In ESPHome, RF switches are typically configured as `template` switches triggering the transmitter action:

```yaml
switch:
  - platform: template
    name: "Living Room Lamp"
    optimistic: true
    turn_on_action:
      - remote_transmitter.transmit_kaku:
          address: 0x01A2B3  # 26-bit address
          unit: 0            # Unit index (0-15)
          state: true
          repeat:
            times: 8
    turn_off_action:
      - remote_transmitter.transmit_kaku:
          address: 0x01A2B3
          unit: 0
          state: false
          repeat:
            times: 8
```

### 2. Transmitter Action (`remote_transmitter.transmit_kaku`)

Use anywhere in automations, scripts, or buttons:

```yaml
# Turn on a specific unit
- remote_transmitter.transmit_kaku:
    address: 0x01A2B3
    unit: 0
    state: true

# Turn off the entire group
- remote_transmitter.transmit_kaku:
    address: 0x01A2B3
    group: true
    state: false

# Set dim level (0-15)
- remote_transmitter.transmit_kaku:
    address: 0x01A2B3
    unit: 0
    dim_level: 8
```

### 3. Binary Sensor (Listening to KAKU Remotes / Sensors)

Trigger binary sensors when a specific remote button is pressed:

```yaml
binary_sensor:
  - platform: remote_receiver
    name: "Wall Switch Button 1"
    kaku:
      address: 0x01A2B3
      unit: 0
      state: true
```

### 4. Automation Trigger & Protocol Dumper

Log incoming packets in the log output:
```yaml
remote_receiver:
  pin: GPIO19
  dump:
    - kaku
```

Trigger automations when any KAKU packet is received:
```yaml
remote_receiver:
  pin: GPIO19
  on_kaku:
    then:
      - logger.log:
          format: "Received KAKU: addr=0x%07X unit=%u state=%s"
          args: [ 'x.address', 'x.unit', 'x.state ? "ON" : "OFF"' ]
```

---

## Protocol Specifications

- **Frequency**: 433.92 MHz ASK / OOK
- **Base period ($T$)**: 260 µs
- **Sync Pulse**: 1$T$ high + 10.5$T$ low (~2730 µs)
- **Stop / Inter-frame Gap**: 1$T$ high + 40$T$ low (~10.4 ms)
- **Bit 1**: $1T$ high, $5T$ low, $1T$ high, $1T$ low
- **Bit 0**: $1T$ high, $1T$ low, $1T$ high, $5T$ low
- **Dim Command**: $1T$ high, $1T$ low, $1T$ high, $1T$ low (followed by 4 dim level bits)
- **Payload layout**: 26 bits address + 1 bit group + 1 bit state/dim + 4 bits unit (+ optional 4 bits dim level)
