/*
 * Route-guidance message registration inside IdentificationInformation.
 * See rgd_identify.h.
 */
#include "rgd_identify.h"

#include <stdio.h>
#include <string.h>

const uint16_t rgd_ident_sent_ids[RGD_IDENT_SENT_COUNT] = { 0x5200, 0x5203 };
const uint16_t rgd_ident_received_ids[RGD_IDENT_RECEIVED_COUNT] = { 0x5201, 0x5202, 0x5204 };

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void put_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

/* Walk the parameters of any iAP2 control frame.  Returns 0 when they tile
 * [6, len) exactly, -1 otherwise.  visit may be NULL. */
static int walk_params(const uint8_t *frame, size_t len,
                       void (*visit)(void *arg, size_t off, size_t plen), void *arg)
{
    size_t off = 6;

    if (!frame || len < 6 || len > 0xffff)
        return -1;
    if (frame[0] != 0x40 || frame[1] != 0x40 || be16(frame + 2) != len)
        return -1;
    while (off < len) {
        size_t plen;
        if (len - off < 4)
            return -1;
        plen = be16(frame + off);
        if (plen < 4 || plen > len - off)
            return -1;
        if (visit)
            visit(arg, off, plen);
        off += plen;
    }
    return 0;
}

struct find_arg {
    const uint8_t *frame;
    uint16_t id;
    size_t off, plen;
    int found;
};

static void find_visit(void *arg, size_t off, size_t plen)
{
    struct find_arg *f = arg;
    if (!f->found && be16(f->frame + off + 2) == f->id) {
        f->off = off;
        f->plen = plen;
        f->found = 1;
    }
}

int rgd_ident_find_param(const uint8_t *frame, size_t len, uint16_t param_id,
                         size_t *off, size_t *plen)
{
    struct find_arg f = { frame, param_id, 0, 0, 0 };

    if (walk_params(frame, len, find_visit, &f) != 0)
        return -1;
    if (f.found) {
        if (off) *off = f.off;
        if (plen) *plen = f.plen;
    }
    return f.found;
}

/* Locate a uint16 list parameter: its data offset and id count. */
static int find_list(const uint8_t *frame, size_t len, uint16_t param_id,
                     size_t *data_off, size_t *count)
{
    size_t off, plen;

    if (rgd_ident_find_param(frame, len, param_id, &off, &plen) != 1)
        return -1;
    if ((plen - 4) % 2 != 0)
        return -1;
    *data_off = off + 4;
    *count = (plen - 4) / 2;
    return 0;
}

static int list_has(const uint8_t *data, size_t count, uint16_t id)
{
    size_t i;
    for (i = 0; i < count; i++)
        if (be16(data + 2 * i) == id)
            return 1;
    return 0;
}

int rgd_ident_missing_ids(const uint8_t *frame, size_t len, uint16_t param_id,
                          const uint16_t *ids, size_t n)
{
    size_t data_off, count, i;
    int missing = 0;

    if (find_list(frame, len, param_id, &data_off, &count) != 0)
        return -1;
    for (i = 0; i < n; i++)
        if (!list_has(frame + data_off, count, ids[i]))
            missing++;
    return missing;
}

size_t rgd_ident_add_ids(uint8_t *frame, size_t len, size_t cap, uint16_t param_id,
                         const uint16_t *ids, size_t n)
{
    size_t data_off, count, end, new_len, i;
    int missing = rgd_ident_missing_ids(frame, len, param_id, ids, n);

    if (missing < 0)
        return 0;
    if (missing == 0)
        return len;
    new_len = len + 2u * (size_t)missing;
    if (new_len > cap || new_len > 0xffff)
        return 0;
    /* Re-derive the list position; rgd_ident_missing_ids validated it. */
    find_list(frame, len, param_id, &data_off, &count);
    end = data_off + 2 * count;
    memmove(frame + end + 2u * (size_t)missing, frame + end, len - end);
    for (i = 0; i < n; i++) {
        /* Check against the original entries only: ids[] has no duplicates,
         * and appended ids sit past count. */
        if (!list_has(frame + data_off, count, ids[i])) {
            put_be16(frame + end, ids[i]);
            end += 2;
        }
    }
    put_be16(frame + data_off - 4, (uint16_t)(4 + (end - data_off)));
    put_be16(frame + 2, (uint16_t)new_len);
    return new_len;
}

/* Bounded "%x,%x,..." appender; marks truncation with "...". */
struct fmt_buf {
    char *out;
    size_t size, used;
    int truncated;
};

static void fmt_hex(struct fmt_buf *b, unsigned v)
{
    int n;

    if (b->truncated || b->size == 0)
        return;
    n = snprintf(b->out + b->used, b->size - b->used, "%s%x", b->used ? "," : "", v);
    if (n < 0 || (size_t)n >= b->size - b->used) {
        b->truncated = 1;
        if (b->size >= 4)
            memcpy(b->out + b->size - 4, "...", 4);
        return;
    }
    b->used += (size_t)n;
}

int rgd_ident_format_ids(const uint8_t *frame, size_t len, uint16_t param_id,
                         char *out, size_t out_size)
{
    struct fmt_buf b = { out, out_size, 0, 0 };
    size_t data_off, count, i;

    if (out_size)
        out[0] = '\0';
    if (find_list(frame, len, param_id, &data_off, &count) != 0)
        return -1;
    for (i = 0; i < count; i++)
        fmt_hex(&b, be16(frame + data_off + 2 * i));
    return (int)count;
}

struct ids_arg {
    const uint8_t *frame;
    struct fmt_buf *b;
    int count;
};

static void ids_visit(void *arg, size_t off, size_t plen)
{
    struct ids_arg *a = arg;
    (void)plen;
    fmt_hex(a->b, be16(a->frame + off + 2));
    a->count++;
}

int rgd_ident_format_param_ids(const uint8_t *frame, size_t len, char *out, size_t out_size)
{
    struct fmt_buf b = { out, out_size, 0, 0 };
    struct ids_arg a = { frame, &b, 0 };

    if (out_size)
        out[0] = '\0';
    if (walk_params(frame, len, ids_visit, &a) != 0) {
        if (out_size)
            out[0] = '\0';   /* drop the partial list */
        return -1;
    }
    return a.count;
}
