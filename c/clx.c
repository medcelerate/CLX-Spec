/*
 * clx.c – CLX Protocol C reference implementation
 *
 * Compile:
 *   gcc -O2 -o clx_example clx.c -lmsgpack-c -Wall -Wextra
 */

#include "clx.h"

#include <msgpack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/*
 * Pack a C-string key then a double value into an open msgpack map.
 */
static void pack_kv_double(msgpack_packer *pk, const char *key, double val)
{
    size_t klen = strlen(key);
    msgpack_pack_str(pk, klen);
    msgpack_pack_str_body(pk, key, klen);
    msgpack_pack_double(pk, val);
}

static void pack_kv_float(msgpack_packer *pk, const char *key, float val)
{
    size_t klen = strlen(key);
    msgpack_pack_str(pk, klen);
    msgpack_pack_str_body(pk, key, klen);
    msgpack_pack_float(pk, val);
}

static void pack_kv_uint8(msgpack_packer *pk, const char *key, uint8_t val)
{
    size_t klen = strlen(key);
    msgpack_pack_str(pk, klen);
    msgpack_pack_str_body(pk, key, klen);
    msgpack_pack_uint8(pk, val);
}

static void pack_kv_uint32(msgpack_packer *pk, const char *key, uint32_t val)
{
    size_t klen = strlen(key);
    msgpack_pack_str(pk, klen);
    msgpack_pack_str_body(pk, key, klen);
    msgpack_pack_uint32(pk, val);
}

static void pack_kv_uint64(msgpack_packer *pk, const char *key, uint64_t val)
{
    size_t klen = strlen(key);
    msgpack_pack_str(pk, klen);
    msgpack_pack_str_body(pk, key, klen);
    msgpack_pack_uint64(pk, val);
}

static void pack_kv_str(msgpack_packer *pk, const char *key, const char *val)
{
    size_t klen = strlen(key);
    size_t vlen = strlen(val);
    msgpack_pack_str(pk, klen);
    msgpack_pack_str_body(pk, key, klen);
    msgpack_pack_str(pk, vlen);
    msgpack_pack_str_body(pk, val, vlen);
}

static void pack_kv_bin(msgpack_packer *pk, const char *key,
                        const uint8_t *data, size_t data_len)
{
    size_t klen = strlen(key);
    msgpack_pack_str(pk, klen);
    msgpack_pack_str_body(pk, key, klen);
    msgpack_pack_bin(pk, data_len);
    msgpack_pack_bin_body(pk, data, data_len);
}

/*
 * Finalise a pack operation: prepend the 1-byte CLX header to the sbuffer
 * contents, copy into a heap buffer, and clean up.  Returns CLX_OK on
 * success; the caller must free *out.
 */
static clx_result_t finalise_packet(uint8_t header,
                                    msgpack_sbuffer *sbuf,
                                    uint8_t **out, size_t *out_len)
{
    *out_len = 1 + sbuf->size;
    *out     = (uint8_t *)malloc(*out_len);
    if (!*out) {
        msgpack_sbuffer_destroy(sbuf);
        return CLX_ERR_OOM;
    }
    (*out)[0] = header;
    memcpy(*out + 1, sbuf->data, sbuf->size);
    msgpack_sbuffer_destroy(sbuf);
    return CLX_OK;
}

/*
 * Locate a key in a msgpack map object and return a pointer to its value,
 * or NULL if not found.
 */
static const msgpack_object *map_get(const msgpack_object_map *map,
                                     const char *key)
{
    size_t klen = strlen(key);
    for (uint32_t i = 0; i < map->size; i++) {
        const msgpack_object_kv *kv = &map->ptr[i];
        if (kv->key.type != MSGPACK_OBJECT_STR)
            continue;
        if (kv->key.via.str.size == klen &&
            memcmp(kv->key.via.str.ptr, key, klen) == 0)
            return &kv->val;
    }
    return NULL;
}

/* Safely copy a msgpack string value into a fixed-size C char buffer. */
static void obj_to_str(const msgpack_object *obj, char *buf, size_t buf_size)
{
    if (!obj || obj->type != MSGPACK_OBJECT_STR) {
        buf[0] = '\0';
        return;
    }
    size_t n = obj->via.str.size < buf_size - 1 ? obj->via.str.size : buf_size - 1;
    memcpy(buf, obj->via.str.ptr, n);
    buf[n] = '\0';
}

/* Extract a double from a float32 or float64 msgpack object. */
static double obj_to_double(const msgpack_object *obj)
{
    if (!obj) return 0.0;
    if (obj->type == MSGPACK_OBJECT_FLOAT32 ||
        obj->type == MSGPACK_OBJECT_FLOAT64)
        return obj->via.f64;
    if (obj->type == MSGPACK_OBJECT_POSITIVE_INTEGER)
        return (double)obj->via.u64;
    if (obj->type == MSGPACK_OBJECT_NEGATIVE_INTEGER)
        return (double)obj->via.i64;
    return 0.0;
}

static uint8_t obj_to_uint8(const msgpack_object *obj)
{
    if (!obj) return 0;
    if (obj->type == MSGPACK_OBJECT_POSITIVE_INTEGER)
        return (uint8_t)obj->via.u64;
    return 0;
}

static uint32_t obj_to_uint32(const msgpack_object *obj)
{
    if (!obj) return 0;
    if (obj->type == MSGPACK_OBJECT_POSITIVE_INTEGER)
        return (uint32_t)obj->via.u64;
    return 0;
}

static uint64_t obj_to_uint64(const msgpack_object *obj)
{
    if (!obj) return 0;
    if (obj->type == MSGPACK_OBJECT_POSITIVE_INTEGER)
        return obj->via.u64;
    return 0;
}

/*
 * Parse the msgpack payload from a raw CLX packet (skips the 1-byte header),
 * checks that the top-level object is a map, and stores the result in *result.
 * Returns CLX_OK on success; the caller must call msgpack_unpacked_destroy().
 */
static clx_result_t parse_map(const uint8_t *raw, size_t len,
                               uint8_t expected_header,
                               msgpack_unpacked *result,
                               const msgpack_object_map **map_out)
{
    if (len < 2 || raw[0] != expected_header)
        return CLX_ERR_INVALID;

    msgpack_unpacked_init(result);
    msgpack_unpack_return ret =
        msgpack_unpack_next(result, (const char *)(raw + 1), len - 1, NULL);
    if (ret != MSGPACK_UNPACK_SUCCESS)
        return CLX_ERR_PARSE;
    if (result->data.type != MSGPACK_OBJECT_MAP)
        return CLX_ERR_PARSE;

    *map_out = &result->data.via.map;
    return CLX_OK;
}

/* -------------------------------------------------------------------------
 * Pack functions
 * ---------------------------------------------------------------------- */

clx_result_t clx_pack_deck(const clx_deck_t *deck,
                            uint8_t **out, size_t *out_len)
{
    msgpack_sbuffer sbuf;
    msgpack_packer  pk;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_map(&pk, 11);
    pack_kv_double(&pk, "Pitch",              deck->pitch);
    pack_kv_double(&pk, "Position",           deck->position);
    pack_kv_double(&pk, "Position2",          deck->position2);
    pack_kv_double(&pk, "NormalizedPosition", deck->norm_position);
    pack_kv_double(&pk, "BPM",                deck->bpm);
    pack_kv_double(&pk, "Length",             deck->length);
    pack_kv_float (&pk, "EQLow",              deck->eq_low);
    pack_kv_float (&pk, "EQMid",              deck->eq_mid);
    pack_kv_float (&pk, "EQHigh",             deck->eq_high);
    pack_kv_uint8 (&pk, "Deck",               deck->deck);
    pack_kv_uint8 (&pk, "Beat",               deck->beat);

    return finalise_packet(CLX_PACKET_DECK, &sbuf, out, out_len);
}

clx_result_t clx_pack_meta(const clx_meta_t *meta,
                            uint8_t **out, size_t *out_len)
{
    msgpack_sbuffer sbuf;
    msgpack_packer  pk;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_map(&pk, 5);
    pack_kv_uint8(&pk, "Deck",     meta->deck);
    pack_kv_str  (&pk, "Title",    meta->title);
    pack_kv_str  (&pk, "Artist",   meta->artist);
    pack_kv_str  (&pk, "Album",    meta->album);
    pack_kv_str  (&pk, "FilePath", meta->filepath);

    return finalise_packet(CLX_PACKET_META, &sbuf, out, out_len);
}

clx_result_t clx_pack_control(const clx_control_t *ctrl,
                               uint8_t **out, size_t *out_len)
{
    msgpack_sbuffer sbuf;
    msgpack_packer  pk;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_map(&pk, 7);
    pack_kv_double(&pk, "UpfaderA",   ctrl->upfader_a);
    pack_kv_double(&pk, "UpfaderB",   ctrl->upfader_b);
    pack_kv_double(&pk, "UpfaderC",   ctrl->upfader_c);
    pack_kv_double(&pk, "UpfaderD",   ctrl->upfader_d);
    pack_kv_double(&pk, "Crossfader", ctrl->crossfader);
    pack_kv_uint8 (&pk, "Active",     ctrl->active);
    pack_kv_str   (&pk, "AppState",   ctrl->app_state);

    return finalise_packet(CLX_PACKET_CONTROL, &sbuf, out, out_len);
}

clx_result_t clx_pack_event(const clx_event_t *evt,
                             uint8_t **out, size_t *out_len)
{
    msgpack_sbuffer sbuf;
    msgpack_packer  pk;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_map(&pk, 2);
    pack_kv_str  (&pk, "Event", evt->event);
    pack_kv_uint8(&pk, "Value", evt->value);

    return finalise_packet(CLX_PACKET_EVENT, &sbuf, out, out_len);
}

clx_result_t clx_pack_binary(const clx_binary_t *bin,
                              uint8_t **out, size_t *out_len)
{
    msgpack_sbuffer sbuf;
    msgpack_packer  pk;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_map(&pk, 5);
    pack_kv_str   (&pk, "Type",  bin->type);
    pack_kv_bin   (&pk, "Hash",  bin->hash, sizeof(bin->hash));
    pack_kv_uint64(&pk, "Total", bin->total);
    pack_kv_uint32(&pk, "Order", bin->order);
    pack_kv_bin   (&pk, "Data",  bin->data, bin->data_len);

    return finalise_packet(CLX_PACKET_BINARY, &sbuf, out, out_len);
}

void clx_pack_waveform_request(uint8_t deck, uint8_t buf[2])
{
    buf[0] = CLX_PACKET_WAVEFORM;
    buf[1] = deck;
}

void clx_pack_magic(uint8_t buf[1])
{
    buf[0] = CLX_PACKET_MAGIC;
}

/* -------------------------------------------------------------------------
 * Unpack functions
 * ---------------------------------------------------------------------- */

clx_result_t clx_unpack_deck(const uint8_t *raw, size_t len,
                              clx_deck_t *deck)
{
    msgpack_unpacked        result;
    const msgpack_object_map *map;
    clx_result_t rc = parse_map(raw, len, CLX_PACKET_DECK, &result, &map);
    if (rc != CLX_OK) return rc;

    memset(deck, 0, sizeof(*deck));
    deck->pitch        = obj_to_double(map_get(map, "Pitch"));
    deck->position     = obj_to_double(map_get(map, "Position"));
    deck->position2    = obj_to_double(map_get(map, "Position2"));
    deck->norm_position= obj_to_double(map_get(map, "NormalizedPosition"));
    deck->bpm          = obj_to_double(map_get(map, "BPM"));
    deck->length       = obj_to_double(map_get(map, "Length"));
    deck->eq_low       = (float)obj_to_double(map_get(map, "EQLow"));
    deck->eq_mid       = (float)obj_to_double(map_get(map, "EQMid"));
    deck->eq_high      = (float)obj_to_double(map_get(map, "EQHigh"));
    deck->deck         = obj_to_uint8(map_get(map, "Deck"));
    deck->beat         = obj_to_uint8(map_get(map, "Beat"));

    msgpack_unpacked_destroy(&result);
    return CLX_OK;
}

clx_result_t clx_unpack_meta(const uint8_t *raw, size_t len,
                              clx_meta_t *meta)
{
    msgpack_unpacked        result;
    const msgpack_object_map *map;
    clx_result_t rc = parse_map(raw, len, CLX_PACKET_META, &result, &map);
    if (rc != CLX_OK) return rc;

    memset(meta, 0, sizeof(*meta));
    meta->deck = obj_to_uint8(map_get(map, "Deck"));
    obj_to_str(map_get(map, "Title"),    meta->title,    sizeof(meta->title));
    obj_to_str(map_get(map, "Artist"),   meta->artist,   sizeof(meta->artist));
    obj_to_str(map_get(map, "Album"),    meta->album,    sizeof(meta->album));
    obj_to_str(map_get(map, "FilePath"), meta->filepath, sizeof(meta->filepath));

    msgpack_unpacked_destroy(&result);
    return CLX_OK;
}

clx_result_t clx_unpack_control(const uint8_t *raw, size_t len,
                                 clx_control_t *ctrl)
{
    msgpack_unpacked        result;
    const msgpack_object_map *map;
    clx_result_t rc = parse_map(raw, len, CLX_PACKET_CONTROL, &result, &map);
    if (rc != CLX_OK) return rc;

    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->upfader_a  = obj_to_double(map_get(map, "UpfaderA"));
    ctrl->upfader_b  = obj_to_double(map_get(map, "UpfaderB"));
    ctrl->upfader_c  = obj_to_double(map_get(map, "UpfaderC"));
    ctrl->upfader_d  = obj_to_double(map_get(map, "UpfaderD"));
    ctrl->crossfader = obj_to_double(map_get(map, "Crossfader"));
    ctrl->active     = obj_to_uint8(map_get(map, "Active"));
    obj_to_str(map_get(map, "AppState"), ctrl->app_state, sizeof(ctrl->app_state));

    msgpack_unpacked_destroy(&result);
    return CLX_OK;
}

clx_result_t clx_unpack_event(const uint8_t *raw, size_t len,
                               clx_event_t *evt)
{
    msgpack_unpacked        result;
    const msgpack_object_map *map;
    clx_result_t rc = parse_map(raw, len, CLX_PACKET_EVENT, &result, &map);
    if (rc != CLX_OK) return rc;

    memset(evt, 0, sizeof(*evt));
    obj_to_str(map_get(map, "Event"), evt->event, sizeof(evt->event));
    evt->value = obj_to_uint8(map_get(map, "Value"));

    msgpack_unpacked_destroy(&result);
    return CLX_OK;
}

clx_result_t clx_unpack_binary(const uint8_t *raw, size_t len,
                                clx_binary_t *bin)
{
    msgpack_unpacked        result;
    const msgpack_object_map *map;
    clx_result_t rc = parse_map(raw, len, CLX_PACKET_BINARY, &result, &map);
    if (rc != CLX_OK) return rc;

    memset(bin, 0, sizeof(*bin));
    obj_to_str(map_get(map, "Type"), bin->type, sizeof(bin->type));
    bin->total = obj_to_uint64(map_get(map, "Total"));
    bin->order = obj_to_uint32(map_get(map, "Order"));

    /* Hash (bin type – fixed 32 bytes for SHA-256) */
    const msgpack_object *hash_obj = map_get(map, "Hash");
    if (hash_obj && hash_obj->type == MSGPACK_OBJECT_BIN) {
        size_t n = hash_obj->via.bin.size < sizeof(bin->hash)
                   ? hash_obj->via.bin.size : sizeof(bin->hash);
        memcpy(bin->hash, hash_obj->via.bin.ptr, n);
    }

    /* Data (bin type – heap-allocated) */
    const msgpack_object *data_obj = map_get(map, "Data");
    if (data_obj && data_obj->type == MSGPACK_OBJECT_BIN &&
        data_obj->via.bin.size > 0)
    {
        bin->data_len = data_obj->via.bin.size;
        bin->data     = (uint8_t *)malloc(bin->data_len);
        if (!bin->data) {
            msgpack_unpacked_destroy(&result);
            return CLX_ERR_OOM;
        }
        memcpy(bin->data, data_obj->via.bin.ptr, bin->data_len);
    }

    msgpack_unpacked_destroy(&result);
    return CLX_OK;
}

void clx_binary_free(clx_binary_t *bin)
{
    if (bin && bin->data) {
        free(bin->data);
        bin->data     = NULL;
        bin->data_len = 0;
    }
}

/* -------------------------------------------------------------------------
 * Minimal usage example (compiled when CLX_EXAMPLE is defined)
 *
 *   gcc -O2 -DCLX_EXAMPLE -o clx_example clx.c -lmsgpack-c -Wall -Wextra
 * ---------------------------------------------------------------------- */
#ifdef CLX_EXAMPLE

int main(void)
{
    uint8_t *buf  = NULL;
    size_t   blen = 0;

    /* --- Deck packet --- */
    clx_deck_t deck = {
        .pitch        = 1.0,
        .position     = 42.5,
        .position2    = 16.25,
        .norm_position= 0.42,
        .bpm          = 128.0,
        .length       = 240.0,
        .eq_low       = 0.0f,
        .eq_mid       = 0.0f,
        .eq_high      = 0.0f,
        .deck         = 0,
        .beat         = 1,
    };
    clx_pack_deck(&deck, &buf, &blen);

    clx_deck_t deck2;
    clx_unpack_deck(buf, blen, &deck2);
    printf("[0x%02X] Deck     → BPM=%.1f  Position=%.2f  Beat=%u\n",
           buf[0], deck2.bpm, deck2.position, deck2.beat);
    free(buf); buf = NULL;

    /* --- Meta packet --- */
    clx_meta_t meta = { .deck = 0 };
    strncpy(meta.title,    "My Track",         sizeof(meta.title)    - 1);
    strncpy(meta.artist,   "My Artist",        sizeof(meta.artist)   - 1);
    strncpy(meta.album,    "My Album",         sizeof(meta.album)    - 1);
    strncpy(meta.filepath, "/music/track.flac",sizeof(meta.filepath) - 1);
    clx_pack_meta(&meta, &buf, &blen);

    clx_meta_t meta2;
    clx_unpack_meta(buf, blen, &meta2);
    printf("[0x%02X] Meta     → Deck=%u  Title=%s  Artist=%s\n",
           buf[0], meta2.deck, meta2.title, meta2.artist);
    free(buf); buf = NULL;

    /* --- Control packet --- */
    clx_control_t ctrl = {
        .upfader_a  = 0.8,
        .upfader_b  = 0.6,
        .upfader_c  = 0.0,
        .upfader_d  = 0.0,
        .crossfader = 0.5,
        .active     = 0,
    };
    strncpy(ctrl.app_state, "Connected", sizeof(ctrl.app_state) - 1);
    clx_pack_control(&ctrl, &buf, &blen);

    clx_control_t ctrl2;
    clx_unpack_control(buf, blen, &ctrl2);
    printf("[0x%02X] Control  → A=%.1f  B=%.1f  Xfader=%.2f  State=%s\n",
           buf[0], ctrl2.upfader_a, ctrl2.upfader_b,
           ctrl2.crossfader, ctrl2.app_state);
    free(buf); buf = NULL;

    /* --- Event packet --- */
    clx_event_t evt = { .value = 0 };
    strncpy(evt.event, "Play", sizeof(evt.event) - 1);
    clx_pack_event(&evt, &buf, &blen);

    clx_event_t evt2;
    clx_unpack_event(buf, blen, &evt2);
    printf("[0x%02X] Event    → Event=%s  Value=%u\n",
           buf[0], evt2.event, evt2.value);
    free(buf); buf = NULL;

    /* --- Binary packet (waveform chunk) --- */
    uint8_t waveform[24];
    for (int i = 0; i < (int)sizeof(waveform); i++) waveform[i] = (uint8_t)i;

    clx_binary_t bin = {
        .total    = sizeof(waveform),
        .order    = 0,
        .data     = waveform,
        .data_len = sizeof(waveform),
    };
    strncpy(bin.type, "waveform", sizeof(bin.type) - 1);
    /* hash field left as zeros for this example */

    clx_pack_binary(&bin, &buf, &blen);

    clx_binary_t bin2;
    clx_unpack_binary(buf, blen, &bin2);
    printf("[0x%02X] Binary   → Type=%s  Order=%u  Total=%llu  DataLen=%zu\n",
           buf[0], bin2.type, bin2.order,
           (unsigned long long)bin2.total, bin2.data_len);
    clx_binary_free(&bin2);
    free(buf); buf = NULL;

    /* --- Waveform request micro-packet --- */
    uint8_t wr[2];
    clx_pack_waveform_request(0, wr);
    printf("[0x%02X] WaveformRequest → deck=%u\n", wr[0], wr[1]);

    /* --- Magic / resync packet --- */
    uint8_t mp[1];
    clx_pack_magic(mp);
    printf("[0x%02X] Magic\n", mp[0]);

    return 0;
}

#endif /* CLX_EXAMPLE */
