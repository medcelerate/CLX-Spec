#pragma once

#include <msgpack11.hpp>
#include <cstdint>
#include <string>
#include <vector>

// CLX packet type headers
static constexpr uint8_t CLX_PACKET_CONTROL  = 0x00;
static constexpr uint8_t CLX_PACKET_DECK     = 0x01;
static constexpr uint8_t CLX_PACKET_META     = 0x02;
static constexpr uint8_t CLX_PACKET_WAVEFORM = 0x03;
static constexpr uint8_t CLX_PACKET_EVENT    = 0x04;
static constexpr uint8_t CLX_PACKET_BINARY   = 0x05;
static constexpr uint8_t CLX_PACKET_MAGIC    = 0x09;

// CLX UDP port
static constexpr uint16_t CLX_PORT          = 3650;
static constexpr uint16_t CLX_WAVEFORM_PORT = 7000;

// ---------------------------------------------------------------------------
// Deck Packet (0x01) — real-time deck state, streamed up to 60 fps
// ---------------------------------------------------------------------------
typedef struct Deck {
    double  velocity      = 0.0;
    double  position      = 0.0;
    double  position2     = 0.0;
    double  norm_position = 0.0;
    double  bpm           = 0.0;
    double  length        = 0.0;
    float   eqlow         = 0.0f;
    float   eqmid         = 0.0f;
    float   eqhigh        = 0.0f;
    uint8_t deck          = 0;
    uint8_t beat          = 0;

    // Deserialize from a MessagePack map
    static Deck fromMsgPack(const msgpack11::MsgPack& msg) {
        Deck d;
        d.velocity      = msg["Pitch"].float64_value();
        d.position      = msg["Position"].float64_value();
        d.position2     = msg["Position2"].float64_value();
        d.norm_position = msg["NormalizedPosition"].float64_value();
        d.bpm           = msg["BPM"].float64_value();
        d.length        = msg["Length"].float64_value();
        d.eqlow         = msg["EQLow"].float32_value();
        d.eqmid         = msg["EQMid"].float32_value();
        d.eqhigh        = msg["EQHigh"].float32_value();
        d.deck          = static_cast<uint8_t>(msg["Deck"].int32_value());
        d.beat          = static_cast<uint8_t>(msg["Beat"].int32_value());
        return d;
    }

    // Serialize to a MessagePack map
    msgpack11::MsgPack toMsgPack() const {
        return msgpack11::MsgPack(msgpack11::MsgPack::object {
            {"Pitch",              msgpack11::MsgPack(velocity)},
            {"Position",          msgpack11::MsgPack(position)},
            {"Position2",         msgpack11::MsgPack(position2)},
            {"NormalizedPosition",msgpack11::MsgPack(norm_position)},
            {"BPM",               msgpack11::MsgPack(bpm)},
            {"Length",            msgpack11::MsgPack(length)},
            {"EQLow",             msgpack11::MsgPack(eqlow)},
            {"EQMid",             msgpack11::MsgPack(eqmid)},
            {"EQHigh",            msgpack11::MsgPack(eqhigh)},
            {"Deck",              msgpack11::MsgPack(static_cast<int>(deck))},
            {"Beat",              msgpack11::MsgPack(static_cast<int>(beat))},
        });
    }

    // Return a complete wire packet: [0x01][msgpack payload]
    std::string packet() const {
        return std::string(1, static_cast<char>(CLX_PACKET_DECK)) + toMsgPack().dump();
    }

    // Deserialize from a complete wire packet (strips the 1-byte header)
    static Deck fromPacket(const std::string& raw) {
        std::string err;
        auto msg = msgpack11::MsgPack::parse(raw.substr(1), err);
        return fromMsgPack(msg);
    }
} Deck_t;

// ---------------------------------------------------------------------------
// Meta Packet (0x02) — track metadata, sent on load or request
// ---------------------------------------------------------------------------
typedef struct Meta {
    uint8_t     deck = 0;
    std::string title;
    std::string artist;
    std::string album;
    std::string filepath;

    static Meta fromMsgPack(const msgpack11::MsgPack& msg) {
        Meta m;
        m.deck     = static_cast<uint8_t>(msg["Deck"].int32_value());
        m.title    = msg["Title"].string_value();
        m.artist   = msg["Artist"].string_value();
        m.album    = msg["Album"].string_value();
        m.filepath = msg["FilePath"].string_value();
        return m;
    }

    msgpack11::MsgPack toMsgPack() const {
        return msgpack11::MsgPack(msgpack11::MsgPack::object {
            {"Deck",     msgpack11::MsgPack(static_cast<int>(deck))},
            {"Title",    msgpack11::MsgPack(title)},
            {"Artist",   msgpack11::MsgPack(artist)},
            {"Album",    msgpack11::MsgPack(album)},
            {"FilePath", msgpack11::MsgPack(filepath)},
        });
    }

    std::string packet() const {
        return std::string(1, static_cast<char>(CLX_PACKET_META)) + toMsgPack().dump();
    }

    static Meta fromPacket(const std::string& raw) {
        std::string err;
        auto msg = msgpack11::MsgPack::parse(raw.substr(1), err);
        return fromMsgPack(msg);
    }
} Meta_t;

// ---------------------------------------------------------------------------
// Control Packet (0x00) — mixer fader and app state
// ---------------------------------------------------------------------------
typedef struct Control {
    double      upfader_a   = 0.0;
    double      upfader_b   = 0.0;
    double      upfader_c   = 0.0;
    double      upfader_d   = 0.0;
    double      crossfader  = 0.0;
    uint8_t     active      = 0;
    std::string state       = "Disconnected";

    static Control fromMsgPack(const msgpack11::MsgPack& msg) {
        Control c;
        c.upfader_a  = msg["UpfaderA"].float64_value();
        c.upfader_b  = msg["UpfaderB"].float64_value();
        c.upfader_c  = msg["UpfaderC"].float64_value();
        c.upfader_d  = msg["UpfaderD"].float64_value();
        c.crossfader = msg["Crossfader"].float64_value();
        c.active     = static_cast<uint8_t>(msg["Active"].int32_value());
        c.state      = msg["AppState"].string_value();
        return c;
    }

    msgpack11::MsgPack toMsgPack() const {
        return msgpack11::MsgPack(msgpack11::MsgPack::object {
            {"UpfaderA",   msgpack11::MsgPack(upfader_a)},
            {"UpfaderB",   msgpack11::MsgPack(upfader_b)},
            {"UpfaderC",   msgpack11::MsgPack(upfader_c)},
            {"UpfaderD",   msgpack11::MsgPack(upfader_d)},
            {"Crossfader", msgpack11::MsgPack(crossfader)},
            {"Active",     msgpack11::MsgPack(static_cast<int>(active))},
            {"AppState",   msgpack11::MsgPack(state)},
        });
    }

    std::string packet() const {
        return std::string(1, static_cast<char>(CLX_PACKET_CONTROL)) + toMsgPack().dump();
    }

    static Control fromPacket(const std::string& raw) {
        std::string err;
        auto msg = msgpack11::MsgPack::parse(raw.substr(1), err);
        return fromMsgPack(msg);
    }
} Control_t;

// ---------------------------------------------------------------------------
// Event Packet (0x04) — client-initiated action or state change
// ---------------------------------------------------------------------------
typedef struct Event {
    std::string event;
    uint8_t     value = 0;

    static Event fromMsgPack(const msgpack11::MsgPack& msg) {
        Event e;
        e.event = msg["Event"].string_value();
        e.value = static_cast<uint8_t>(msg["Value"].int32_value());
        return e;
    }

    msgpack11::MsgPack toMsgPack() const {
        return msgpack11::MsgPack(msgpack11::MsgPack::object {
            {"Event", msgpack11::MsgPack(event)},
            {"Value", msgpack11::MsgPack(static_cast<int>(value))},
        });
    }

    std::string packet() const {
        return std::string(1, static_cast<char>(CLX_PACKET_EVENT)) + toMsgPack().dump();
    }

    static Event fromPacket(const std::string& raw) {
        std::string err;
        auto msg = msgpack11::MsgPack::parse(raw.substr(1), err);
        return fromMsgPack(msg);
    }
} Event_t;

// ---------------------------------------------------------------------------
// Binary Packet (0x05) — chunked binary payload (e.g. waveform data)
//
// The Data field uses the MessagePack bin type (not str) so arbitrary bytes
// can be carried without UTF-8 validation overhead.
// ---------------------------------------------------------------------------
typedef struct BinaryPacket {
    std::string              type;
    std::vector<uint8_t>     hash;   // SHA-256 hash of the full dataset (bin type)
    uint64_t                 total  = 0;
    uint32_t                 order  = 0;
    std::vector<uint8_t>     data;   // raw binary payload (bin type)

    static BinaryPacket fromMsgPack(const msgpack11::MsgPack& msg) {
        BinaryPacket bp;
        bp.type  = msg["Type"].string_value();
        bp.hash  = msg["Hash"].binary_items();
        bp.total = static_cast<uint64_t>(msg["Total"].int32_value());
        bp.order = static_cast<uint32_t>(msg["Order"].int32_value());
        bp.data  = msg["Data"].binary_items();
        return bp;
    }

    msgpack11::MsgPack toMsgPack() const {
        msgpack11::MsgPack::binary hash_bin(hash.begin(), hash.end());
        msgpack11::MsgPack::binary data_bin(data.begin(), data.end());
        return msgpack11::MsgPack(msgpack11::MsgPack::object {
            {"Type",  msgpack11::MsgPack(type)},
            {"Hash",  msgpack11::MsgPack(hash_bin)},
            {"Total", msgpack11::MsgPack(static_cast<int>(total))},
            {"Order", msgpack11::MsgPack(static_cast<int>(order))},
            {"Data",  msgpack11::MsgPack(data_bin)},
        });
    }

    std::string packet() const {
        return std::string(1, static_cast<char>(CLX_PACKET_BINARY)) + toMsgPack().dump();
    }

    static BinaryPacket fromPacket(const std::string& raw) {
        std::string err;
        auto msg = msgpack11::MsgPack::parse(raw.substr(1), err);
        return fromMsgPack(msg);
    }
} BinaryPacket_t;

// ---------------------------------------------------------------------------
// Waveform Request (0x03) — two-byte micro-packet: [0x03][deck uint8]
//
// Send to port CLX_WAVEFORM_PORT (7000) on the source device.
// ---------------------------------------------------------------------------
inline std::string clx_waveform_request(uint8_t deck) {
    return std::string({static_cast<char>(CLX_PACKET_WAVEFORM),
                        static_cast<char>(deck)});
}

// ---------------------------------------------------------------------------
// Magic Packet (0x09) — broadcast on join to trigger full metadata resync
// ---------------------------------------------------------------------------
inline std::string clx_magic_packet() {
    return std::string(1, static_cast<char>(CLX_PACKET_MAGIC));
}
