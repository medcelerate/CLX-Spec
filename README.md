# CLX

# 🎛️ MessagePack Packet Format: DJ Deck State Sync

This document describes the structure of serialized packets used for syncing real-time DJ playback data between clients and servers. All packets are sent over the network as:

```
[1-byte Packet Type Header][MessagePack Payload]
```

---

## 📄 MessagePack Schema Notes

Due to hardware memory limitations, MessagePack payloads are assumed to have a schema-less encoding  scheme.  The first and only top-level object is a map of Key-Value pairs, similar to a JSON object.  This is how libraries like `MPack`, `msgpack11`, and `msgpack23` serialize data by default.

This allows for arbitrary field/schema ordering and the ability to add backwards compatability for additional fields without the need for a proper protocol versioning system.  However, due to limited memory constraints, it must be assumed that hardware devices using CLX will only parse the first object of a MessagePack payload, and only if it is a Map of Key/Value objects.  This allows for a much lower-level, recursion-free parsing scheme on hardware using `msgpack-c`.

---

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
| `Position2`         | `float64`| Beatgrid position (`Beat Number.fraction`)       |
| `NormalizedPosition`| `float64`| Position normalized to range [0.0–1.0] |
| `BPM`               | `float64`| Current BPM of the track               |
| `Length`            | `float64`| Track duration (in seconds)            |
| `EQLow`             | `float32`| Low EQ gain level                      |
| `EQMid`             | `float32`| Mid EQ gain level                      |
| `EQHigh`            | `float32`| High EQ gain level                     |
| `Deck`              | `uint8`  | Deck index (e.g., 0 = A, 1 = B)        |
| `Beat`              | `uint8`  | Current Beat (1-4) all other values should be ignored |

---

## 📝 Metadata Packet (`0x02`)

Track metadata, typically sent once on load or when requested. All strings UTF-8 encoded.

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

A binary packet containing waveform data. Each packet is a maximum size of `4096` bytes. Different from the other packet types this is constructed as follows:

| Offset  | Size | Value                  | Description                                     |
|---------|------|------------------------|-------------------------------------------------|
| 1       | 32   | File Name (MD5 Hash)   | MD5 hash that acts as the file name (ascii hex) |
| 33      |  8   | BBC Payload Total Size | Total size of file expected (uint64 LE)         |
| 41      |  4   | Order                  | Ordering of received data (uint32 LE)           |
| 45      |  N   | Data                   | Binary data                                     |

We follow the conventions from the BBC, with one alteration, appeneded to the bottom is a CLRS section in binary containing the rgb color values for each pair of values. This is represented as clrs in the json format.
https://github.com/bbc/audiowaveform/blob/master/doc/DataFormat.md

## Binary Data (`0x05`)

A binary packet containing arbitrary data. The below fields describe an example used for waveform V2 data. Additonal fields are optional depending on how the data needs to be used. It is recommended to include an order value as well as the expected total size.

| Key     | Type     | Description                    |
|---------|----------|--------------------------------|
| `Type`  | `string` | Type of data e.g waveform      |
| `Hash`  | `bytes`  | sha256 hash of entire dataset  |
| `Total` | `uint64` | Total size of payload          |
| `Order` | `uint32` | Packet Order                   |
| `Data`  | `binary` | Binary Data                    |


## 🧠 Behavioral Notes

- Clients **broadcast a magic packet** upon joining to request full metadata resync (`Meta`, `Control`) `0x09`.
- `Deck` packets are streamed continuously and may be throttled to 60fps for performance.
- `Event` packets are **commands**, not state — the actions they define are user definable.
- Servers are expected to track and respond based on `Deck`, `Meta`, and `Control` states.

---
