"""
CLX Protocol – Python reference implementation
===============================================
Uses the ``msgpack`` library for the fastest MessagePack serialisation.

Install dependency:
    pip install msgpack

Wire format per packet:
    [1-byte Packet Type Header][MessagePack map payload]

Special packets:
    Waveform Request (0x03): [0x03][deck uint8]  (two bytes, no msgpack)
    Magic / resync  (0x09): [0x09]               (one byte, no msgpack)
"""

import socket
import struct
from typing import Optional, Tuple

import msgpack  # pip install msgpack

# ---------------------------------------------------------------------------
# Packet type constants
# ---------------------------------------------------------------------------
PACKET_CONTROL  = 0x00
PACKET_DECK     = 0x01
PACKET_META     = 0x02
PACKET_WAVEFORM = 0x03
PACKET_EVENT    = 0x04
PACKET_BINARY   = 0x05
PACKET_MAGIC    = 0x09

CLX_PORT          = 3650
CLX_WAVEFORM_PORT = 7000
MULTICAST_ADDR    = "239.0.0.1"


# ---------------------------------------------------------------------------
# Packer — serialise CLX packets as bytes ready to send over UDP
# ---------------------------------------------------------------------------
class CLXPacker:
    """Serialises CLX packet structs to bytes using msgpack.

    A single ``Packer`` instance is reused across calls so the internal
    buffer is not reallocated on every call.
    """

    def __init__(self) -> None:
        # use_bin_type=True ensures Python bytes / bytearray are encoded as
        # the msgpack *bin* type rather than the legacy *raw* type.
        self._packer = msgpack.Packer(use_bin_type=True)

    def _pack(self, packet_type: int, data: dict) -> bytes:
        return bytes([packet_type]) + self._packer.pack(data)

    def pack_deck(
        self,
        pitch: float,
        position: float,
        position2: float,
        norm_position: float,
        bpm: float,
        length: float,
        eq_low: float,
        eq_mid: float,
        eq_high: float,
        deck: int,
        beat: int,
    ) -> bytes:
        """Deck packet (0x01) – real-time deck state, streamed up to 60 fps."""
        return self._pack(PACKET_DECK, {
            "Pitch":              pitch,
            "Position":           position,
            "Position2":          position2,
            "NormalizedPosition": norm_position,
            "BPM":                bpm,
            "Length":             length,
            "EQLow":              eq_low,
            "EQMid":              eq_mid,
            "EQHigh":             eq_high,
            "Deck":               deck,
            "Beat":               beat,
        })

    def pack_meta(
        self,
        deck: int,
        title: str,
        artist: str,
        album: str,
        filepath: str,
    ) -> bytes:
        """Meta packet (0x02) – track metadata, sent on load or request."""
        return self._pack(PACKET_META, {
            "Deck":     deck,
            "Title":    title,
            "Artist":   artist,
            "Album":    album,
            "FilePath": filepath,
        })

    def pack_control(
        self,
        upfader_a: float,
        upfader_b: float,
        upfader_c: float,
        upfader_d: float,
        crossfader: float,
        active: int,
        app_state: str,
    ) -> bytes:
        """Control packet (0x00) – mixer fader and app state."""
        return self._pack(PACKET_CONTROL, {
            "UpfaderA":   upfader_a,
            "UpfaderB":   upfader_b,
            "UpfaderC":   upfader_c,
            "UpfaderD":   upfader_d,
            "Crossfader": crossfader,
            "Active":     active,
            "AppState":   app_state,
        })

    def pack_event(self, event: str, value: int) -> bytes:
        """Event packet (0x04) – client-initiated action."""
        return self._pack(PACKET_EVENT, {
            "Event": event,
            "Value": value,
        })

    def pack_binary(
        self,
        data_type: str,
        hash_bytes: bytes,
        total: int,
        order: int,
        data: bytes,
    ) -> bytes:
        """Binary packet (0x05) – chunked binary payload (e.g. waveform).

        ``hash_bytes`` and ``data`` are encoded as msgpack *bin* type so
        arbitrary bytes can be carried without UTF-8 restrictions.
        """
        return self._pack(PACKET_BINARY, {
            "Type":  data_type,
            "Hash":  hash_bytes,   # bytes → msgpack bin
            "Total": total,
            "Order": order,
            "Data":  data,         # bytes → msgpack bin
        })

    def pack_waveform_request(self, deck: int) -> bytes:
        """Waveform request (0x03) – two-byte micro-packet, no msgpack body."""
        return bytes([PACKET_WAVEFORM, deck & 0xFF])

    def pack_magic(self) -> bytes:
        """Magic packet (0x09) – broadcast on join to trigger full resync."""
        return bytes([PACKET_MAGIC])


# ---------------------------------------------------------------------------
# Unpacker — deserialise incoming CLX packets
# ---------------------------------------------------------------------------
class CLXUnpacker:
    """Deserialises raw CLX bytes into (packet_type, payload_dict) tuples.

    ``raw=False``     → decode msgpack *str* type as Python ``str``
    ``use_list=False``→ decode msgpack arrays as tuples (slightly faster)
    """

    def unpack(self, data: bytes) -> Tuple[Optional[int], Optional[dict]]:
        """Return (packet_type, payload).

        For Waveform Request returns (PACKET_WAVEFORM, {"Deck": n}).
        For Magic returns (PACKET_MAGIC, None).
        Returns (None, None) on empty input.
        """
        if not data:
            return None, None

        packet_type = data[0]

        if packet_type == PACKET_WAVEFORM:
            deck = data[1] if len(data) > 1 else 0
            return packet_type, {"Deck": deck}

        if packet_type == PACKET_MAGIC:
            return packet_type, None

        payload = msgpack.unpackb(data[1:], raw=False, use_list=False)
        return packet_type, payload


# ---------------------------------------------------------------------------
# UDP Socket helper
# ---------------------------------------------------------------------------
class CLXSocket:
    """Thin UDP socket wrapper for sending and receiving CLX packets.

    Usage (receiver):
        sock = CLXSocket()
        sock.join_multicast()      # optional – receive multicast
        while True:
            ptype, payload, addr = sock.recv()

    Usage (sender):
        sock = CLXSocket(bind_port=0)   # ephemeral port for send-only
        sock.send(packer.pack_deck(...), "255.255.255.255")
    """

    def __init__(
        self,
        bind_address: str = "0.0.0.0",
        bind_port: int = CLX_PORT,
    ) -> None:
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        # Binding to 0.0.0.0 is intentional: CLX packets arrive via unicast,
        # broadcast, and multicast, so we must listen on all interfaces.
        self._sock.bind((bind_address, bind_port))  # nosec B104
        self._packer   = CLXPacker()
        self._unpacker = CLXUnpacker()

    def join_multicast(self, multicast_addr: str = MULTICAST_ADDR) -> None:
        """Join the CLX multicast group (239.0.0.1) to receive multicast."""
        mreq = struct.pack("4sL", socket.inet_aton(multicast_addr), socket.INADDR_ANY)
        self._sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    def send(self, data: bytes, address: str, port: int = CLX_PORT) -> None:
        self._sock.sendto(data, (address, port))

    def recv(
        self, buffer_size: int = 65536
    ) -> Tuple[Optional[int], Optional[dict], Optional[tuple]]:
        """Receive one CLX packet.

        Returns (packet_type, payload_dict, (host, port)).
        """
        raw, addr = self._sock.recvfrom(buffer_size)
        packet_type, payload = self._unpacker.unpack(raw)
        return packet_type, payload, addr

    @property
    def packer(self) -> CLXPacker:
        return self._packer

    def close(self) -> None:
        self._sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()


# ---------------------------------------------------------------------------
# Minimal usage example
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    import hashlib

    packer   = CLXPacker()
    unpacker = CLXUnpacker()

    # --- Deck packet ---
    deck_raw = packer.pack_deck(
        pitch=1.0, position=42.5, position2=16.25,
        norm_position=0.42, bpm=128.0, length=240.0,
        eq_low=0.0, eq_mid=0.0, eq_high=0.0,
        deck=0, beat=1,
    )
    ptype, payload = unpacker.unpack(deck_raw)
    print(f"[0x{ptype:02X}] Deck     → {payload}")

    # --- Meta packet ---
    meta_raw = packer.pack_meta(0, "My Track", "My Artist", "My Album", "/music/track.flac")
    ptype, payload = unpacker.unpack(meta_raw)
    print(f"[0x{ptype:02X}] Meta     → {payload}")

    # --- Control packet ---
    ctrl_raw = packer.pack_control(0.8, 0.6, 0.0, 0.0, 0.5, 0, "Connected")
    ptype, payload = unpacker.unpack(ctrl_raw)
    print(f"[0x{ptype:02X}] Control  → {payload}")

    # --- Event packet ---
    evt_raw = packer.pack_event("Play", 0)
    ptype, payload = unpacker.unpack(evt_raw)
    print(f"[0x{ptype:02X}] Event    → {payload}")

    # --- Binary packet (waveform chunk) ---
    waveform_bytes = bytes(range(24))   # placeholder waveform data
    sha256_hash    = hashlib.sha256(waveform_bytes).digest()
    bin_raw = packer.pack_binary(
        data_type="waveform",
        hash_bytes=sha256_hash,
        total=len(waveform_bytes),
        order=0,
        data=waveform_bytes,
    )
    ptype, payload = unpacker.unpack(bin_raw)
    print(
        f"[0x{ptype:02X}] Binary   → Type={payload['Type']!r}, "
        f"Order={payload['Order']}, Total={payload['Total']}, "
        f"DataLen={len(payload['Data'])}, HashLen={len(payload['Hash'])}"
    )

    # --- Waveform request micro-packet ---
    wr_raw = packer.pack_waveform_request(deck=0)
    ptype, payload = unpacker.unpack(wr_raw)
    print(f"[0x{ptype:02X}] WaveformRequest → {payload}")

    # --- Magic / resync packet ---
    magic_raw = packer.pack_magic()
    ptype, payload = unpacker.unpack(magic_raw)
    print(f"[0x{ptype:02X}] Magic    → {payload}")
