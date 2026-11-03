/*
 * HTTP API pure core — no ESP-IDF includes (architecture.md).
 * Basic-auth check (§22) and a streaming multipart/form-data parser that
 * hands the bytes of one named field to a sink (POST /api/ota, FR-11.3).
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* `header` is the Authorization value ("Basic <b64>"). True iff it carries
 * exactly user:pass. An empty pass never matches. */
bool http_basic_auth_ok(const char *header, const char *user, const char *pass);

/* Extract the boundary parameter of a multipart/form-data Content-Type.
 * Returns false when absent or longer than out_len - 1. */
bool http_multipart_boundary(const char *content_type, char *out, size_t out_len);

#define MP_BOUNDARY_MAX 70
#define MP_HDR_MAX      512

/* Sink for the selected field's bytes; return false to abort parsing. */
typedef bool (*mp_sink_t)(const uint8_t *data, size_t len, void *ctx);

typedef enum { MP_MORE = 0, MP_DONE = 1, MP_ERROR = -1 } mp_status_t;

typedef struct {
    uint8_t  delim[4 + MP_BOUNDARY_MAX];   /* "\r\n--" + boundary */
    uint8_t  fail[4 + MP_BOUNDARY_MAX];    /* KMP failure table   */
    size_t   dlen;
    size_t   j;                            /* delimiter bytes matched */
    int      phase;                        /* internal */
    char     hdr[MP_HDR_MAX];
    size_t   hlen;
    uint8_t  after[2];
    size_t   alen;
    bool     capture;                      /* current part is the field */
    bool     found;                        /* the field was seen (complete) */
    char     field[32];
    mp_sink_t sink;
    void    *ctx;
    uint8_t  out[256];
    size_t   olen;
} mp_parser_t;

/* Returns false if the boundary is too long or the field name too long. */
bool mp_init(mp_parser_t *p, const char *boundary, const char *field, mp_sink_t sink, void *ctx);

/* Feed body bytes. MP_DONE once the closing delimiter is read and the field
 * was found; MP_ERROR on malformed input, sink refusal, or a closing
 * delimiter without the field. */
mp_status_t mp_feed(mp_parser_t *p, const uint8_t *data, size_t len);
