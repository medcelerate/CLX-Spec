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
| `0x03`     | Waveform    | Waveform Request                              |
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
| `UpfaderC`  | `float64`  | Fader level for Deck C                   |
| `UpfaderD`  | `float64`  | Fader level for Deck D                   |
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

## Binary Data (`0x05`)

A binary packet containing arbitrary data. The below fields describe an example used for waveform V2 data. Additonal fields are optional depending on how the data needs to be used. It is recommended to include an order value as well as the expected total size.

| Key     | Type     | Description                    |
|---------|----------|--------------------------------|
| `Type`  | `string` | Type of data e.g waveform      |
| `Hash`  | `bytes`  | sha256 hash of entire dataset  |
| `Total` | `uint64` | Total size of payload          |
| `Order` | `uint32` | Packet Order                   |
| `Data`  | `binary` | Binary Data                    |

We follow the conventions from the BBC, with one alteration, appended to the bottom is a CLRS section in binary containing the rgb color values for each pair of values. This is represented as clrs in the json format.
https://github.com/bbc/audiowaveform/blob/master/doc/DataFormat.md

Waveform data comes in as binary data in fragements. The re-assembled data is a messsagepack blob as below. These should be saved as rwf files in a local cache.
| Key     | Type     | Description                    |
|---------|----------|--------------------------------|
| `Data`  | `string` | The binary of the waveform data|
| `Hash`  | `bytes`  | md5 sum of track title, waveform file name  |


## Waveform Request (`0x03`)
This is a special form of micro-packet that is used for clients that support retransmission of waveform. This is simply two bytes, the `header` and the deck stored as a uint8. This will trigger transmission of the current loaded waveform on the source. CLX Senders can receive this on port 7000 specifically.


## 🧠 Behavioral Notes

- Clients **broadcast a magic packet** upon joining to request full metadata resync (`Meta`, `Control`) `0x09`.
- `Deck` packets are streamed continuously and may be throttled to 60fps for performance.
- `Event` packets are **commands**, not state — the actions they define are user definable.
- Servers are expected to track and respond based on `Deck`, `Meta`, and `Control` states.
- If you are a spectator it is highly recommended to implement both unicast and multicast listen. Multicast happens on address `239.0.0.1`

---
