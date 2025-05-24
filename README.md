# ECNet

# 🎛️ MessagePack Packet Format: DJ Deck State Sync

This document describes the structure of serialized packets used for syncing real-time DJ playback data between clients and servers. All packets are sent over the network as:

```
[1-byte Packet Type Header][MessagePack Payload]
```

---

## 📦 Packet Type Headers

| Byte Value | Packet Type | Description                                  |
|------------|-------------|----------------------------------------------|
| `0x01`     | Deck        | Real-time deck data (sent up to 60fps)       |
| `0x02`     | Meta        | Track metadata (sent on load or event)       |
| `0x03`     | Control     | Mixer and control state                      |
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
| `Deck`              | `int32`  | Deck index (e.g., 0 = A, 1 = B)        |

---

## 📝 Metadata Packet (`0x02`)

Track metadata, typically sent once on load or when requested.

| Key         | Type       | Description                          |
|-------------|------------|--------------------------------------|
| `Deck`      | `int32`    | Deck number                          |
| `Title`     | `string`   | Track title                          |
| `Artist`    | `string`   | Track artist                         |
| `Album`     | `string`   | Track album                          |
| `FilePath`  | `string`   | Path to the loaded track file        |

---

## 🎛️ Control Packet (`0x03`)

Represents mixer fader states and app state.

| Key         | Type       | Description                              |
|-------------|------------|------------------------------------------|
| `UpfaderA`  | `float64`  | Fader level for Deck A                   |
| `UpfaderB`  | `float64`  | Fader level for Deck B                   |
| `Crossfader`| `float64`  | Crossfader position (typically -1 to 1) |
| `Active`    | `int32`    | Active deck or focus status             |
| `AppState`  | `string`   | App connection or session state         |

---

## 🎯 Event Packet (`0x04`)

Signals a client-initiated action or state change.

| Key     | Type     | Description                              |
|---------|----------|------------------------------------------|
| `Event` | `string` | Event name (e.g., "Load", "Cue", "Play") |
| `Value` | `uint8`  | Optional numeric value for the event     |

---

## 🧠 Behavioral Notes

- Clients **broadcast a magic packet** upon joining to request full metadata resync (`Meta`, `Control`).
- `Deck` packets are streamed continuously and may be throttled to 60fps or lower for performance.
- `Event` packets are **commands**, not state — they may trigger metadata refresh or playback actions.
- Servers are expected to track and respond based on `Deck`, `Meta`, and `Control` states.

---

Let me know if you'd like this exported to a `.md` or `.pdf`!
