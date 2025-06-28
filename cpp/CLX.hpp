#include <msgpack11>

typedef struct Deck {
    double velocity = 0;
    double position = 0;
    double position2 = 0;
    double norm_position = 0;
    double bpm = 0;
    double length = 0;
    float eqlow = 0;
    float eqmid = 0;
    float eqhigh = 0;
    int deck = 0;
    int beat = 0;
    
    static Deck fromMsgPack(const msgpack11::MsgPack& msg) {
        Deck deck;
        deck.velocity = msg["Pitch"].float64_value();
        deck.position = msg["Position"].float64_value();
        deck.position2 = msg["Position2"].float64_value();
        deck.norm_position = msg["NormalizedPosition"].float64_value();
        deck.bpm = msg["BPM"].float64_value();
        deck.length = msg["Length"].float64_value();
        deck.eqlow = msg["EQLow"].float32_value();
        deck.eqmid = msg["EQMid"].float32_value();
        deck.eqhigh = msg["EQHigh"].float32_value();
        deck.deck = msg["Deck"].uint8_value();
        deck.beat = msg["Beat"].int32_value();
        return deck;
    }
        
} Deck_t;

typedef struct Meta {
    int deck;
    std::string title;
    std::string artist;
    std::string album;
    std::string filepath;

    static Meta fromMsgPack(const msgpack11::MsgPack& msg) {
        Meta meta;
        meta.deck = msg["Deck"].uint8_value();
        meta.title = msg["Title"].string_value();
        meta.artist = msg["Artist"].string_value();
        meta.album = msg["Album"].string_value();
        meta.filepath = msg["FilePath"].string_value();
        return meta;
    }
} Meta_t;


typedef struct Control {
    double upfader_a = 0;
    double upfader_b = 0;
    double crossfader = 0;
    int active;
    std::string state = "Disconnected";
    
    static Control fromMsgPack(const msgpack11::MsgPack& msg) {
        Control control;
        control.upfader_a = msg["UpfaderA"].float64_value();
        control.upfader_b = msg["UpfaderB"].float64_value();
        control.crossfader = msg["Crossfader"].float64_value();
        control.active = msg["Active"].int32_value();
        control.state = msg["AppState"].string_value();
        return control;
    }
} Control_t;

typedef struct Event {
    std::string event;
    uint8_t value;
    static Event fromMsgPack(const msgpack11::MsgPack& msg) {
        Event event;
        event.event = msg["Event"].string_value();
        event.value = msg["Value"].uint8_value();
        return event;
    }
        
} Event_t;
