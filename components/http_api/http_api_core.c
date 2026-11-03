/* HTTP API pure core (FSD §11, §22). No ESP-IDF includes. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "http_api_core.h"

#include <string.h>
#include <strings.h>

/* ---- Basic auth --------------------------------------------------------- */

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t b64_encode(const uint8_t *in, size_t n, char *out, size_t cap)
{
    size_t o = 0;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < n) v |= (uint32_t)in[i + 1] << 8;
        if (i + 2 < n) v |= in[i + 2];
        if (o + 4 >= cap) return 0;
        out[o++] = B64[(v >> 18) & 63];
        out[o++] = B64[(v >> 12) & 63];
        out[o++] = i + 1 < n ? B64[(v >> 6) & 63] : '=';
        out[o++] = i + 2 < n ? B64[v & 63] : '=';
    }
    out[o] = '\0';
    return o;
}

bool http_basic_auth_ok(const char *header, const char *user, const char *pass)
{
    if (!header || !user || !pass || !pass[0]) return false;
    if (strncasecmp(header, "Basic ", 6) != 0) return false;
    const char *got = header + 6;
    while (*got == ' ') got++;

    uint8_t plain[32 + 1 + 64];
    size_t ul = strlen(user), pl = strlen(pass);
    if (ul + 1 + pl > sizeof(plain)) return false;
    memcpy(plain, user, ul);
    plain[ul] = ':';
    memcpy(plain + ul + 1, pass, pl);

    char want[4 * ((sizeof(plain) + 2) / 3) + 1];
    size_t wl = b64_encode(plain, ul + 1 + pl, want, sizeof(want));
    if (wl == 0 || strlen(got) != wl) return false;
    unsigned diff = 0;                      /* no early exit on the first mismatch */
    for (size_t i = 0; i < wl; i++) diff |= (unsigned)(got[i] ^ want[i]);
    return diff == 0;
}

/* ---- multipart ---------------------------------------------------------- */

bool http_multipart_boundary(const char *ct, char *out, size_t out_len)
{
    if (!ct || strncasecmp(ct, "multipart/form-data", 19) != 0) return false;
    const char *b = strcasestr(ct, "boundary=");
    if (!b) return false;
    b += 9;
    bool quoted = (*b == '"');
    if (quoted) b++;
    size_t n = 0;
    while (b[n] && (quoted ? b[n] != '"' : (b[n] != ';' && b[n] != ' '))) n++;
    if (n == 0 || n >= out_len || n > MP_BOUNDARY_MAX) return false;
    memcpy(out, b, n);
    out[n] = '\0';
    return true;
}

enum { PH_PREAMBLE, PH_AFTER, PH_HEADERS, PH_BODY, PH_END };

bool mp_init(mp_parser_t *p, const char *boundary, const char *field, mp_sink_t sink, void *ctx)
{
    size_t bl = strlen(boundary);
    if (bl == 0 || bl > MP_BOUNDARY_MAX || strlen(field) >= sizeof(p->field)) return false;
    memset(p, 0, sizeof(*p));
    memcpy(p->delim, "\r\n--", 4);
    memcpy(p->delim + 4, boundary, bl);
    p->dlen = 4 + bl;
    /* KMP failure table */
    p->fail[0] = 0;
    for (size_t i = 1, k = 0; i < p->dlen; i++) {
        while (k > 0 && p->delim[i] != p->delim[k]) k = p->fail[k - 1];
        if (p->delim[i] == p->delim[k]) k++;
        p->fail[i] = (uint8_t)k;
    }
    strcpy(p->field, field);
    p->sink = sink;
    p->ctx = ctx;
    p->phase = PH_PREAMBLE;
    /* The first delimiter has no leading CRLF: pretend we already saw it. */
    p->j = 2;
    return true;
}

static bool flush(mp_parser_t *p)
{
    bool ok = true;
    if (p->olen && p->capture) ok = p->sink(p->out, p->olen, p->ctx);
    p->olen = 0;
    return ok;
}

static bool emit(mp_parser_t *p, const uint8_t *d, size_t n)
{
    if (p->phase != PH_BODY || !p->capture) return true;
    for (size_t i = 0; i < n; i++) {
        p->out[p->olen++] = d[i];
        if (p->olen == sizeof(p->out) && !flush(p)) return false;
    }
    return true;
}

static void headers_done(mp_parser_t *p)
{
    char pat[48];
    size_t fl = strlen(p->field);
    memcpy(pat, "name=\"", 6);
    memcpy(pat + 6, p->field, fl);
    pat[6 + fl] = '"';
    pat[7 + fl] = '\0';
    p->hdr[p->hlen] = '\0';
    p->capture = strcasestr(p->hdr, "content-disposition") && strstr(p->hdr, pat);
}

mp_status_t mp_feed(mp_parser_t *p, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];
        switch (p->phase) {
        case PH_PREAMBLE:
        case PH_BODY:
            while (p->j > 0 && p->delim[p->j] != c) {
                size_t k = p->fail[p->j - 1];
                if (!emit(p, p->delim, p->j - k)) return MP_ERROR;
                p->j = k;
            }
            if (p->delim[p->j] == c) {
                if (++p->j == p->dlen) {          /* delimiter complete */
                    if (!flush(p)) return MP_ERROR;
                    if (p->phase == PH_BODY && p->capture) p->found = true;
                    p->capture = false;
                    p->j = 0;
                    p->alen = 0;
                    p->phase = PH_AFTER;
                }
            } else if (!emit(p, &c, 1)) {
                return MP_ERROR;
            }
            break;
        case PH_AFTER:
            p->after[p->alen++] = c;
            if (p->alen == 2) {
                if (p->after[0] == '-' && p->after[1] == '-') {
                    p->phase = PH_END;
                    return p->found ? MP_DONE : MP_ERROR;
                }
                if (p->after[0] != '\r' || p->after[1] != '\n') return MP_ERROR;
                p->hlen = 0;
                p->phase = PH_HEADERS;
            }
            break;
        case PH_HEADERS:
            if (p->hlen >= sizeof(p->hdr) - 1) return MP_ERROR;
            p->hdr[p->hlen++] = (char)c;
            if (p->hlen >= 4 && memcmp(p->hdr + p->hlen - 4, "\r\n\r\n", 4) == 0) {
                headers_done(p);
                p->phase = PH_BODY;
            }
            break;
        case PH_END:
            return p->found ? MP_DONE : MP_ERROR;
        }
    }
    return MP_MORE;
}
