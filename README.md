# ECNet

# 🎛️ MessagePack Packet Format: DJ Deck State Sync

This document describes the structure of serialized packets used for syncing real-time DJ playback data between clients and servers. All packets are sent over the network as:

```
[1-byte Packet Type Header][MessagePack Payload]
```

The data is exposed as udp unicast, broadcast, or multicast over port `3650`. If using unicast it is recommended to offer a "hop" from the receiver to another network.

---

## 📦 Packet Type Headers

| Byte Value | Packet Type | Description                                  |
|------------|-------------|----------------------------------------------|
| `0x01`     | Deck        | Real-time deck data (sent up to 60fps)       |
| `0x02`     | Meta        | Track metadata (sent on load or event)       |
| `0x00`     | Control     | Mixer and control state                      |
| `0x03`     | Waveform    | Waveform Binary                              |
| `0x04`     | Event       | Event trigger (e.g., load, cue, play toggle) |

---

## 🎚️ Deck Packet (`0x01`)

Represents the real-time state of a single playback deck.

| Key                 | Type     | Description                            |
|---------------------|----------|----------------------------------------|
| `Pitch`             | `float64`| Track pitch or velocity                |
| `Position`          | `float64`| Current playhead position (in seconds) |
| `Position2`         | `float64`| Alternate or secondary position        |
| `NormalizedPosition`| `float64`| Position normalized to range [0.0–1.0] |
| `BPM`               | `float64`| Current BPM of the track               |
| `Length`            | `float64`| Track duration (in seconds)            |
| `EQLow`             | `float32`| Low EQ gain level                      |
| `EQMid`             | `float32`| Mid EQ gain level                      |
| `EQHigh`            | `float32`| High EQ gain level                     |
| `Deck`              | `uint8`  | Deck index (e.g., 0 = A, 1 = B)        |

---

## 📝 Metadata Packet (`0x02`)

Track metadata, typically sent once on load or when requested.

| Key         | Type       | Description                          |
|-------------|------------|--------------------------------------|
| `Deck`      | `uint8`    | Deck number                          |
| `Title`     | `string`   | Track title                          |
| `Artist`    | `string`   | Track artist                         |
| `Album`     | `string`   | Track album                          |
| `FilePath`  | `string`   | Path to the loaded track file        |

---

## 🎛️ Control Packet (`0x00`)

Represents mixer fader states and app state.

| Key         | Type       | Description                              |
|-------------|------------|------------------------------------------|
| `UpfaderA`  | `float64`  | Fader level for Deck A                   |
| `UpfaderB`  | `float64`  | Fader level for Deck B                   |
| `Crossfader`| `float64`  | Crossfader position (typically 0 to 1)   |
| `Active`    | `uint8`    | Active deck or focus status              |
| `AppState`  | `string`   | App connection or session state          |

---

## 🎯 Event Packet (`0x04`)

Signals a client-initiated action or state change.

| Key     | Type     | Description                              |
|---------|----------|------------------------------------------|
| `Event` | `string` | Event name (e.g., "Load", "Cue", "Play") |
| `Value` | `uint8`  | Optional numeric value for the event     |

---

## Waveform Data (`0x03`)

A binary packet containing waveform data. Each packet is a maximum size of `1024` bytes. Different from the other packet types this is constructed as follows:

| Offset  | Size | Value                | Description                              |
|---------|------|----------------------|------------------------------------------|
| 1       | 32   | File Name (MD5 Hash) | MD5 hash that acts as the file name |
| 33      |  8   | Total Size           | Total size of file expected  |
| 40      |  4   | Order                | Ordering of received data   |
| 44      |  N   | Data                 | Binary data  |

## 🧠 Behavioral Notes

- Clients **broadcast a magic packet** upon joining to request full metadata resync (`Meta`, `Control`) `0x09`.
- `Deck` packets are streamed continuously and may be throttled to 60fps for performance.
- `Event` packets are **commands**, not state — the actions they define are user definable.
- Servers are expected to track and respond based on `Deck`, `Meta`, and `Control` states.

---
