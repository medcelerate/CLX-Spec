// Example shows how to encode and decode every CLX packet type.
//
// Run:
//
//	go run ./example
package main

import (
	"crypto/sha256"
	"fmt"
	"log"

	clx "github.com/medcelerate/CLX-Spec/go"
)

func main() {
	// --- Deck packet ---
	deck := clx.DeckPacket{
		Pitch:              1.0,
		Position:           42.5,
		Position2:          16.25,
		NormalizedPosition: 0.42,
		BPM:                128.0,
		Length:             240.0,
		EQLow:              0.0,
		EQMid:              0.0,
		EQHigh:             0.0,
		Deck:               0,
		Beat:               1,
	}
	raw, err := clx.Pack(clx.PacketDeck, &deck)
	checkErr(err)
	var deckOut clx.DeckPacket
	header, err := clx.Unpack(raw, &deckOut)
	checkErr(err)
	fmt.Printf("[0x%02X] Deck     → BPM=%.1f  Position=%.2f  Beat=%d\n",
		header, deckOut.BPM, deckOut.Position, deckOut.Beat)

	// --- Meta packet ---
	meta := clx.MetaPacket{
		Deck:     0,
		Title:    "My Track",
		Artist:   "My Artist",
		Album:    "My Album",
		FilePath: "/music/track.flac",
	}
	raw, err = clx.Pack(clx.PacketMeta, &meta)
	checkErr(err)
	var metaOut clx.MetaPacket
	header, err = clx.Unpack(raw, &metaOut)
	checkErr(err)
	fmt.Printf("[0x%02X] Meta     → Deck=%d  Title=%q  Artist=%q\n",
		header, metaOut.Deck, metaOut.Title, metaOut.Artist)

	// --- Control packet ---
	ctrl := clx.ControlPacket{
		UpfaderA:   0.8,
		UpfaderB:   0.6,
		UpfaderC:   0.0,
		UpfaderD:   0.0,
		Crossfader: 0.5,
		Active:     0,
		AppState:   "Connected",
	}
	raw, err = clx.Pack(clx.PacketControl, &ctrl)
	checkErr(err)
	var ctrlOut clx.ControlPacket
	header, err = clx.Unpack(raw, &ctrlOut)
	checkErr(err)
	fmt.Printf("[0x%02X] Control  → A=%.1f  B=%.1f  Xfader=%.2f  State=%q\n",
		header, ctrlOut.UpfaderA, ctrlOut.UpfaderB, ctrlOut.Crossfader, ctrlOut.AppState)

	// --- Event packet ---
	evt := clx.EventPacket{Event: "Play", Value: 0}
	raw, err = clx.Pack(clx.PacketEvent, &evt)
	checkErr(err)
	var evtOut clx.EventPacket
	header, err = clx.Unpack(raw, &evtOut)
	checkErr(err)
	fmt.Printf("[0x%02X] Event    → Event=%q  Value=%d\n",
		header, evtOut.Event, evtOut.Value)

	// --- Binary packet (waveform chunk) ---
	waveformData := make([]byte, 24)
	for i := range waveformData {
		waveformData[i] = byte(i)
	}
	hash := sha256.Sum256(waveformData)
	bin := clx.BinaryPacket{
		Type:  "waveform",
		Hash:  hash[:],
		Total: uint64(len(waveformData)),
		Order: 0,
		Data:  waveformData,
	}
	raw, err = clx.Pack(clx.PacketBinary, &bin)
	checkErr(err)
	var binOut clx.BinaryPacket
	header, err = clx.Unpack(raw, &binOut)
	checkErr(err)
	fmt.Printf("[0x%02X] Binary   → Type=%q  Order=%d  Total=%d  DataLen=%d  HashLen=%d\n",
		header, binOut.Type, binOut.Order, binOut.Total, len(binOut.Data), len(binOut.Hash))

	// --- Waveform request micro-packet ---
	wr := clx.WaveformRequest(0)
	fmt.Printf("[0x%02X] WaveformRequest → deck=%d\n", wr[0], wr[1])

	// --- Magic / resync packet ---
	mp := clx.MagicPacket()
	fmt.Printf("[0x%02X] Magic\n", mp[0])
}

func checkErr(err error) {
	if err != nil {
		log.Fatal(err)
	}
}
