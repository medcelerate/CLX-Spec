// Package clx is a reference implementation of the CLX Protocol for
// real-time DJ deck state synchronisation.
//
// Wire format per packet:
//
//	[1-byte Packet Type Header][MessagePack map payload]
//
// Special packets:
//
//	Waveform Request (0x03): [0x03][deck uint8]  – two bytes, no msgpack body
//	Magic / resync   (0x09): [0x09]              – one byte, no msgpack body
//
// Serialisation uses github.com/vmihailenco/msgpack/v5, which is the most
// widely used Go MessagePack library and performs struct-tag–based map
// encoding with minimal allocations.
//
// Install:
//
//	go get github.com/vmihailenco/msgpack/v5
package clx

import (
	"fmt"
	"net"

	"github.com/vmihailenco/msgpack/v5"
)

// ---------------------------------------------------------------------------
// Packet type constants
// ---------------------------------------------------------------------------

const (
	PacketControl  byte = 0x00
	PacketDeck     byte = 0x01
	PacketMeta     byte = 0x02
	PacketWaveform byte = 0x03
	PacketEvent    byte = 0x04
	PacketBinary   byte = 0x05
	PacketMagic    byte = 0x09
)

const (
	CLXPort       = 3650
	WaveformPort  = 7000
	MulticastAddr = "239.0.0.1"
)

// ---------------------------------------------------------------------------
// Packet structs
// ---------------------------------------------------------------------------

// DeckPacket represents the real-time state of a single playback deck (0x01).
// Streamed at up to 60 fps.
type DeckPacket struct {
	Pitch              float64 `msgpack:"Pitch"`
	Position           float64 `msgpack:"Position"`
	Position2          float64 `msgpack:"Position2"`
	NormalizedPosition float64 `msgpack:"NormalizedPosition"`
	BPM                float64 `msgpack:"BPM"`
	Length             float64 `msgpack:"Length"`
	EQLow              float32 `msgpack:"EQLow"`
	EQMid              float32 `msgpack:"EQMid"`
	EQHigh             float32 `msgpack:"EQHigh"`
	Deck               uint8   `msgpack:"Deck"`
	Beat               uint8   `msgpack:"Beat"`
}

// MetaPacket carries track metadata (0x02). Sent once on load or on request.
// All strings are UTF-8 encoded.
type MetaPacket struct {
	Deck     uint8  `msgpack:"Deck"`
	Title    string `msgpack:"Title"`
	Artist   string `msgpack:"Artist"`
	Album    string `msgpack:"Album"`
	FilePath string `msgpack:"FilePath"`
}

// ControlPacket represents mixer fader states and app state (0x00).
type ControlPacket struct {
	UpfaderA   float64 `msgpack:"UpfaderA"`
	UpfaderB   float64 `msgpack:"UpfaderB"`
	UpfaderC   float64 `msgpack:"UpfaderC"`
	UpfaderD   float64 `msgpack:"UpfaderD"`
	Crossfader float64 `msgpack:"Crossfader"`
	Active     uint8   `msgpack:"Active"`
	AppState   string  `msgpack:"AppState"`
}

// EventPacket signals a client-initiated action or state change (0x04).
type EventPacket struct {
	Event string `msgpack:"Event"`
	Value uint8  `msgpack:"Value"`
}

// BinaryPacket carries a chunked binary payload such as waveform data (0x05).
// Hash and Data are encoded as the MessagePack bin type.
type BinaryPacket struct {
	Type  string `msgpack:"Type"`
	Hash  []byte `msgpack:"Hash"`  // SHA-256 of the full dataset (bin type)
	Total uint64 `msgpack:"Total"` // Total size of the complete payload
	Order uint32 `msgpack:"Order"` // Chunk index
	Data  []byte `msgpack:"Data"`  // Raw binary chunk (bin type)
}

// ---------------------------------------------------------------------------
// Serialisation helpers
// ---------------------------------------------------------------------------

// Pack prepends the 1-byte CLX header to a msgpack-serialised struct.
func Pack(header byte, v any) ([]byte, error) {
	payload, err := msgpack.Marshal(v)
	if err != nil {
		return nil, err
	}
	buf := make([]byte, 1+len(payload))
	buf[0] = header
	copy(buf[1:], payload)
	return buf, nil
}

// Unpack reads the 1-byte CLX header then deserialises the msgpack body into v.
func Unpack(data []byte, v any) (byte, error) {
	if len(data) < 2 {
		return 0, fmt.Errorf("clx: packet too short (%d bytes)", len(data))
	}
	return data[0], msgpack.Unmarshal(data[1:], v)
}

// WaveformRequest returns the two-byte waveform request micro-packet.
func WaveformRequest(deck uint8) []byte { return []byte{PacketWaveform, deck} }

// MagicPacket returns the single-byte magic / resync packet.
func MagicPacket() []byte { return []byte{PacketMagic} }

// ---------------------------------------------------------------------------
// UDP socket helper
// ---------------------------------------------------------------------------

// Socket wraps a UDP connection for sending and receiving CLX packets.
type Socket struct {
	conn *net.UDPConn
}

// Listen creates a UDP socket bound to addr:port ready to receive packets.
// Use addr="0.0.0.0" to listen on all interfaces.
func Listen(addr string, port int) (*Socket, error) {
	udpAddr, err := net.ResolveUDPAddr("udp4", fmt.Sprintf("%s:%d", addr, port))
	if err != nil {
		return nil, err
	}
	conn, err := net.ListenUDP("udp4", udpAddr)
	if err != nil {
		return nil, err
	}
	return &Socket{conn: conn}, nil
}

// ListenMulticast creates a UDP socket and joins the CLX multicast group.
func ListenMulticast(iface *net.Interface) (*Socket, error) {
	group, err := net.ResolveUDPAddr("udp4", fmt.Sprintf("%s:%d", MulticastAddr, CLXPort))
	if err != nil {
		return nil, err
	}
	conn, err := net.ListenMulticastUDP("udp4", iface, group)
	if err != nil {
		return nil, err
	}
	return &Socket{conn: conn}, nil
}

// Send transmits raw CLX packet bytes to addr:port.
func (s *Socket) Send(data []byte, addr string, port int) error {
	dst, err := net.ResolveUDPAddr("udp4", fmt.Sprintf("%s:%d", addr, port))
	if err != nil {
		return err
	}
	_, err = s.conn.WriteTo(data, dst)
	return err
}

// Recv reads a raw CLX packet and returns its bytes plus the sender address.
// buf should be at least 65536 bytes to handle the largest UDP datagrams.
func (s *Socket) Recv(buf []byte) (int, net.Addr, error) {
	return s.conn.ReadFrom(buf)
}

// Close closes the underlying UDP connection.
func (s *Socket) Close() error { return s.conn.Close() }
