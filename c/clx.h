/*
 * clx.h – CLX Protocol C reference implementation
 *
 * Uses msgpack-c for serialisation (https://github.com/msgpack/msgpack-c).
 *
 * Install (Debian/Ubuntu):
 *   apt-get install libmsgpack-dev
 *
 * Compile example:
 *   gcc -O2 -o clx_example clx.c -lmsgpack-c -Wall -Wextra
 *
 * Wire format per packet:
 *   [1-byte Packet Type Header][MessagePack map payload]
 *
 * Special packets:
 *   Waveform Request (0x03): [0x03][deck uint8]  – two bytes, no msgpack body
 *   Magic / resync   (0x09): [0x09]              – one byte, no msgpack body
 */

#ifndef CLX_H
#define CLX_H

#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Packet type constants
 * ---------------------------------------------------------------------- */
#define CLX_PACKET_CONTROL   0x00
#define CLX_PACKET_DECK      0x01
#define CLX_PACKET_META      0x02
#define CLX_PACKET_WAVEFORM  0x03
#define CLX_PACKET_EVENT     0x04
#define CLX_PACKET_BINARY    0x05
#define CLX_PACKET_MAGIC     0x09

#define CLX_PORT            3650
#define CLX_WAVEFORM_PORT   7000

/* -------------------------------------------------------------------------
 * String / buffer size limits used by value-type struct fields.
 * Applications needing longer strings should use dynamic allocation.
 * ---------------------------------------------------------------------- */
#define CLX_MAX_STR      256
#define CLX_MAX_FILEPATH 1024

/* -------------------------------------------------------------------------
 * Return codes
 * ---------------------------------------------------------------------- */
typedef enum {
    CLX_OK            =  0,
    CLX_ERR_OOM       = -1,   /* memory allocation failed                  */
    CLX_ERR_INVALID   = -2,   /* bad header or unexpected packet type       */
    CLX_ERR_PARSE     = -3,   /* msgpack parse error                        */
    CLX_ERR_ENCODE    = -4,   /* msgpack encode error                       */
} clx_result_t;

/* -------------------------------------------------------------------------
 * Packet structs
 * ---------------------------------------------------------------------- */

/* Deck Packet (0x01) – real-time deck state, streamed up to 60 fps */
typedef struct {
    double  pitch;
    double  position;
    double  position2;
    double  norm_position;
    double  bpm;
    double  length;
    float   eq_low;
    float   eq_mid;
    float   eq_high;
    uint8_t deck;
    uint8_t beat;
} clx_deck_t;

/* Meta Packet (0x02) – track metadata, sent on load or request */
typedef struct {
    uint8_t deck;
    char    title[CLX_MAX_STR];
    char    artist[CLX_MAX_STR];
    char    album[CLX_MAX_STR];
    char    filepath[CLX_MAX_FILEPATH];
} clx_meta_t;

/* Control Packet (0x00) – mixer fader and app state */
typedef struct {
    double  upfader_a;
    double  upfader_b;
    double  upfader_c;
    double  upfader_d;
    double  crossfader;
    uint8_t active;
    char    app_state[CLX_MAX_STR];
} clx_control_t;

/* Event Packet (0x04) – client-initiated action */
typedef struct {
    char    event[CLX_MAX_STR];
    uint8_t value;
} clx_event_t;

/*
 * Binary Packet (0x05) – chunked binary payload (e.g. waveform data).
 *
 * 'hash' and 'data' use the MessagePack bin type so arbitrary bytes can be
 * carried without UTF-8 restrictions.  The caller owns 'data' and must free
 * it with clx_binary_free() when done.
 */
typedef struct {
    char     type[CLX_MAX_STR];
    uint8_t  hash[32];     /* SHA-256 of the complete dataset */
    uint64_t total;        /* total size of the complete payload */
    uint32_t order;        /* chunk index */
    uint8_t *data;         /* heap-allocated binary chunk (caller must free) */
    size_t   data_len;
} clx_binary_t;

/* -------------------------------------------------------------------------
 * Pack functions
 *
 * Each function allocates a buffer (*out) containing the full CLX wire
 * packet (1-byte header + msgpack map).  The caller is responsible for
 * freeing *out with free().
 * ---------------------------------------------------------------------- */

clx_result_t clx_pack_deck(const clx_deck_t *deck,
                            uint8_t **out, size_t *out_len);

clx_result_t clx_pack_meta(const clx_meta_t *meta,
                            uint8_t **out, size_t *out_len);

clx_result_t clx_pack_control(const clx_control_t *ctrl,
                               uint8_t **out, size_t *out_len);

clx_result_t clx_pack_event(const clx_event_t *evt,
                             uint8_t **out, size_t *out_len);

clx_result_t clx_pack_binary(const clx_binary_t *bin,
                              uint8_t **out, size_t *out_len);

/*
 * Waveform request micro-packet: writes exactly 2 bytes into buf[0..1].
 * buf must be at least 2 bytes.
 */
void clx_pack_waveform_request(uint8_t deck, uint8_t buf[2]);

/*
 * Magic / resync packet: writes 1 byte into buf[0].
 * buf must be at least 1 byte.
 */
void clx_pack_magic(uint8_t buf[1]);

/* -------------------------------------------------------------------------
 * Unpack functions
 *
 * Each function validates the 1-byte header then deserialises the msgpack
 * body.  For clx_unpack_binary, the 'data' field is heap-allocated; call
 * clx_binary_free() when done.
 * ---------------------------------------------------------------------- */

clx_result_t clx_unpack_deck(const uint8_t *raw, size_t len,
                              clx_deck_t *deck);

clx_result_t clx_unpack_meta(const uint8_t *raw, size_t len,
                              clx_meta_t *meta);

clx_result_t clx_unpack_control(const uint8_t *raw, size_t len,
                                 clx_control_t *ctrl);

clx_result_t clx_unpack_event(const uint8_t *raw, size_t len,
                               clx_event_t *evt);

clx_result_t clx_unpack_binary(const uint8_t *raw, size_t len,
                                clx_binary_t *bin);

/* Free the heap-allocated 'data' field inside a clx_binary_t. */
void clx_binary_free(clx_binary_t *bin);

#endif /* CLX_H */
