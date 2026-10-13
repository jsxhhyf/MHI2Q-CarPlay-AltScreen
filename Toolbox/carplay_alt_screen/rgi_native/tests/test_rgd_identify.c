/*
 * Host test for routeguidance/rgd_identify.c.  Run by Tools/test_rgd_identify.sh
 * (cc with ASan + UBSan).  Exit status = number of failed checks.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/routeguidance/rgd_identify.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
    failures++; } } while (0)

/* ---- frame builder ---------------------------------------------------- */

typedef struct { uint8_t b[2048]; size_t len; } frame_t;

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static unsigned get16(const uint8_t *p) { return (unsigned)(p[0] << 8 | p[1]); }

static void f_begin(frame_t *f)
{
    memset(f, 0, sizeof(*f));
    f->b[0] = 0x40; f->b[1] = 0x40;
    put16(f->b + 4, 0x1d01);
    f->len = 6;
}

static void f_param(frame_t *f, unsigned id, const uint8_t *data, size_t n)
{
    put16(f->b + f->len, (unsigned)(4 + n));
    put16(f->b + f->len + 2, id);
    if (n) memcpy(f->b + f->len + 4, data, n);
    f->len += 4 + n;
}

static void f_list(frame_t *f, unsigned id, const unsigned *ids, size_t n)
{
    uint8_t data[256];
    size_t i;
    for (i = 0; i < n; i++) put16(data + 2 * i, ids[i]);
    f_param(f, id, data, 2 * n);
}

static void f_end(frame_t *f) { put16(f->b + 2, (unsigned)f->len); }

/* An Identify shaped like a real accessory one: name/model/vendor strings,
 * the two message lists, power, then a component group. */
static const unsigned base_sent[] = { 0x5000, 0x5002, 0xffff, 0xae00, 0xae02, 0x4e01 };
static const unsigned base_recv[] = { 0x5001, 0xae01, 0xfffe, 0xfffc, 0x4e09 };
static const unsigned rg_sent[] = { 0x5200, 0x5203 };
static const unsigned rg_recv[] = { 0x5201, 0x5202, 0x5204 };

static void build(frame_t *f, int with_rg_sent, int with_rg_recv, int one_sent_only)
{
    unsigned sent[16], recv[16];
    size_t ns = 0, nr = 0, i;
    static const uint8_t name[] = "MMI\0";
    static const uint8_t comp[] = { 0x00, 0x06, 0x00, 0x00, 0x00, 0x01 };

    for (i = 0; i < sizeof(base_sent) / sizeof(base_sent[0]); i++) sent[ns++] = base_sent[i];
    if (with_rg_sent) sent[ns++] = 0x5200;
    if (with_rg_sent && !one_sent_only) sent[ns++] = 0x5203;
    for (i = 0; i < sizeof(base_recv) / sizeof(base_recv[0]); i++) recv[nr++] = base_recv[i];
    if (with_rg_recv) { recv[nr++] = 0x5201; recv[nr++] = 0x5202; recv[nr++] = 0x5204; }

    f_begin(f);
    f_param(f, 0, name, sizeof(name));
    f_param(f, 1, name, sizeof(name));
    f_list(f, 6, sent, ns);
    f_list(f, 7, recv, nr);
    f_param(f, 8, (const uint8_t *)"\x00", 1);
    f_param(f, 0x16, comp, sizeof(comp));
    f_end(f);
}

/* ---- helpers ---------------------------------------------------------- */

static int list_ids(const frame_t *f, unsigned pid, unsigned *out, size_t max)
{
    size_t off, plen, i, n;
    if (rgd_ident_find_param(f->b, f->len, (uint16_t)pid, &off, &plen) != 1) return -1;
    n = (plen - 4) / 2;
    for (i = 0; i < n && i < max; i++) out[i] = get16(f->b + off + 4 + 2 * i);
    return (int)n;
}

static size_t patch_both(frame_t *f, size_t cap)
{
    size_t l = rgd_ident_add_ids(f->b, f->len, cap, IDENT_PARAM_MESSAGES_SENT,
                                 rgd_ident_sent_ids, RGD_IDENT_SENT_COUNT);
    if (!l) return 0;
    f->len = l;
    l = rgd_ident_add_ids(f->b, f->len, cap, IDENT_PARAM_MESSAGES_RECEIVED,
                          rgd_ident_received_ids, RGD_IDENT_RECEIVED_COUNT);
    if (!l) return 0;
    f->len = l;
    return l;
}

/* Every parameter other than 6/7 must survive byte-identical and in order. */
static int others_equal(const frame_t *a, const frame_t *b)
{
    size_t oa = 6, ob = 6;
    for (;;) {
        while (oa < a->len && (get16(a->b + oa + 2) == 6 || get16(a->b + oa + 2) == 7)) oa += get16(a->b + oa);
        while (ob < b->len && (get16(b->b + ob + 2) == 6 || get16(b->b + ob + 2) == 7)) ob += get16(b->b + ob);
        if (oa >= a->len || ob >= b->len) return oa >= a->len && ob >= b->len;
        if (get16(a->b + oa) != get16(b->b + ob) || memcmp(a->b + oa, b->b + ob, get16(a->b + oa))) return 0;
        oa += get16(a->b + oa);
        ob += get16(b->b + ob);
    }
}

/* ---- tests ------------------------------------------------------------ */

/* The module must register exactly what the installer writes to json. */
static void test_constants(void)
{
    size_t i;
    CHECK(RGD_IDENT_SENT_COUNT == sizeof(rg_sent) / sizeof(rg_sent[0]));
    CHECK(RGD_IDENT_RECEIVED_COUNT == sizeof(rg_recv) / sizeof(rg_recv[0]));
    for (i = 0; i < RGD_IDENT_SENT_COUNT; i++) CHECK(rgd_ident_sent_ids[i] == rg_sent[i]);
    for (i = 0; i < RGD_IDENT_RECEIVED_COUNT; i++) CHECK(rgd_ident_received_ids[i] == rg_recv[i]);
}

static void test_cn_unchanged(void)
{
    frame_t f, orig;
    build(&f, 1, 1, 0);
    orig = f;
    CHECK(rgd_ident_missing_ids(f.b, f.len, 6, rgd_ident_sent_ids, 2) == 0);
    CHECK(rgd_ident_missing_ids(f.b, f.len, 7, rgd_ident_received_ids, 3) == 0);
    CHECK(patch_both(&f, sizeof(f.b)) == orig.len);
    CHECK(f.len == orig.len && memcmp(f.b, orig.b, f.len) == 0);
}

static void test_er_appended(void)
{
    frame_t f, orig;
    unsigned ids[32];
    int n, i;
    size_t nb = sizeof(base_sent) / sizeof(base_sent[0]), nr = sizeof(base_recv) / sizeof(base_recv[0]);

    build(&f, 0, 0, 0);
    orig = f;
    CHECK(rgd_ident_missing_ids(f.b, f.len, 6, rgd_ident_sent_ids, 2) == 2);
    CHECK(rgd_ident_missing_ids(f.b, f.len, 7, rgd_ident_received_ids, 3) == 3);
    CHECK(patch_both(&f, sizeof(f.b)) == orig.len + 10);
    CHECK(get16(f.b + 2) == f.len);
    CHECK(rgd_ident_find_param(f.b, f.len, 0x16, NULL, NULL) == 1);   /* still tiles */
    n = list_ids(&f, 6, ids, 32);
    CHECK(n == (int)nb + 2);
    for (i = 0; i < (int)nb && i < n; i++) CHECK(ids[i] == base_sent[i]);   /* order kept */
    CHECK(n >= 2 && ids[n - 2] == 0x5200 && ids[n - 1] == 0x5203);
    n = list_ids(&f, 7, ids, 32);
    CHECK(n == (int)nr + 3);
    CHECK(n >= 3 && ids[n - 3] == 0x5201 && ids[n - 2] == 0x5202 && ids[n - 1] == 0x5204);
    CHECK(others_equal(&f, &orig));
    CHECK(rgd_ident_missing_ids(f.b, f.len, 6, rgd_ident_sent_ids, 2) == 0);
    CHECK(rgd_ident_missing_ids(f.b, f.len, 7, rgd_ident_received_ids, 3) == 0);
    {   /* idempotent */
        frame_t again = f;
        CHECK(patch_both(&again, sizeof(again.b)) == f.len);
        CHECK(memcmp(again.b, f.b, f.len) == 0);
    }
}

static void test_partial(void)
{
    frame_t f;
    unsigned ids[32];
    int n;
    build(&f, 1, 1, 1);            /* 0x5200 present, 0x5203 missing */
    CHECK(rgd_ident_missing_ids(f.b, f.len, 6, rgd_ident_sent_ids, 2) == 1);
    {
        size_t before = f.len;
        CHECK(patch_both(&f, sizeof(f.b)) == before + 2);
    }
    n = list_ids(&f, 6, ids, 32);
    CHECK(n > 0 && ids[n - 1] == 0x5203);
    CHECK(n > 1 && ids[n - 2] == 0x5200);
}

static void test_cap_short(void)
{
    frame_t f, orig;
    build(&f, 0, 0, 0);
    orig = f;
    CHECK(rgd_ident_add_ids(f.b, f.len, f.len + 3, 6, rgd_ident_sent_ids, 2) == 0);
    CHECK(memcmp(f.b, orig.b, sizeof(f.b)) == 0);
    CHECK(rgd_ident_add_ids(f.b, f.len, f.len + 4, 6, rgd_ident_sent_ids, 2) == f.len + 4);
}

static void test_malformed(void)
{
    frame_t f, orig;
    char s[64];

    build(&f, 0, 0, 0);
    put16(f.b + 2, (unsigned)f.len - 1);                       /* header length lies */
    orig = f;
    CHECK(rgd_ident_missing_ids(f.b, f.len, 6, rgd_ident_sent_ids, 2) == -1);
    CHECK(rgd_ident_add_ids(f.b, f.len, sizeof(f.b), 6, rgd_ident_sent_ids, 2) == 0);
    CHECK(memcmp(f.b, orig.b, sizeof(f.b)) == 0);

    build(&f, 0, 0, 0);
    f.b[0] = 0x41;                                             /* bad sync */
    CHECK(rgd_ident_find_param(f.b, f.len, 6, NULL, NULL) == -1);

    build(&f, 0, 0, 0);
    put16(f.b + 6, 3);                                         /* param length < 4 */
    CHECK(rgd_ident_find_param(f.b, f.len, 6, NULL, NULL) == -1);

    build(&f, 0, 0, 0);
    put16(f.b + 6, 0x400);                                     /* param overruns */
    CHECK(rgd_ident_find_param(f.b, f.len, 6, NULL, NULL) == -1);

    build(&f, 0, 0, 0);                                        /* 3 trailing bytes */
    f.len += 3; f_end(&f);
    CHECK(rgd_ident_find_param(f.b, f.len, 6, NULL, NULL) == -1);

    f_begin(&f);                                               /* odd list length */
    f_param(&f, 6, (const uint8_t *)"\x52\x00\x01", 3);
    f_list(&f, 7, base_recv, 2);
    f_end(&f);
    orig = f;
    CHECK(rgd_ident_missing_ids(f.b, f.len, 6, rgd_ident_sent_ids, 2) == -1);
    CHECK(rgd_ident_add_ids(f.b, f.len, sizeof(f.b), 6, rgd_ident_sent_ids, 2) == 0);
    CHECK(memcmp(f.b, orig.b, sizeof(f.b)) == 0);
    CHECK(rgd_ident_format_ids(f.b, f.len, 6, s, sizeof(s)) == -1 && s[0] == '\0');

    f_begin(&f);                                               /* list 7 absent */
    f_list(&f, 6, base_sent, 2);
    f_end(&f);
    orig = f;
    CHECK(rgd_ident_find_param(f.b, f.len, 7, NULL, NULL) == 0);
    CHECK(rgd_ident_missing_ids(f.b, f.len, 7, rgd_ident_received_ids, 3) == -1);
    CHECK(rgd_ident_add_ids(f.b, f.len, sizeof(f.b), 7, rgd_ident_received_ids, 3) == 0);
    CHECK(memcmp(f.b, orig.b, sizeof(f.b)) == 0);

    CHECK(rgd_ident_find_param(NULL, 0, 6, NULL, NULL) == -1);
    CHECK(rgd_ident_find_param(f.b, 5, 6, NULL, NULL) == -1);
}

static void test_format(void)
{
    frame_t f;
    char s[256], tiny[12];
    build(&f, 0, 0, 0);
    patch_both(&f, sizeof(f.b));
    CHECK(rgd_ident_format_ids(f.b, f.len, 6, s, sizeof(s)) == 8);
    CHECK(strcmp(s, "5000,5002,ffff,ae00,ae02,4e01,5200,5203") == 0);
    CHECK(rgd_ident_format_ids(f.b, f.len, 7, tiny, sizeof(tiny)) == 8);
    CHECK(strlen(tiny) < sizeof(tiny) && strcmp(tiny + strlen(tiny) - 3, "...") == 0);

    /* IdentificationRejected naming parameters 6 and 0x1e. */
    f_begin(&f);
    put16(f.b + 4, 0x1d03);
    f_param(&f, 6, NULL, 0);
    f_param(&f, 0x1e, NULL, 0);
    f_end(&f);
    CHECK(rgd_ident_format_param_ids(f.b, f.len, s, sizeof(s)) == 2);
    CHECK(strcmp(s, "6,1e") == 0);
    f_begin(&f);
    f_end(&f);
    CHECK(rgd_ident_format_param_ids(f.b, f.len, s, sizeof(s)) == 0 && s[0] == '\0');
}

/* Random corruption: never crash (ASan/UBSan watch), never accept a result
 * that does not re-parse, never touch a buffer it rejected. */
static void test_fuzz(void)
{
    unsigned seed = 0x1d01;
    int round;
    for (round = 0; round < 20000; round++) {
        frame_t f, orig;
        int k, flips;
        size_t cap, r;
        char s[64];
        build(&f, round & 1, round & 2, round & 4);
        seed = seed * 1103515245u + 12345u;
        flips = 1 + (int)((seed >> 16) % 4);
        for (k = 0; k < flips; k++) {
            seed = seed * 1103515245u + 12345u;
            f.b[(seed >> 8) % f.len] ^= (uint8_t)(1u << ((seed >> 4) % 8));
        }
        seed = seed * 1103515245u + 12345u;
        if (seed & 0x100) { f.len -= (seed >> 12) % 7; if (f.len < 2) f.len = 2; }
        cap = f.len + (seed >> 20) % 16;
        orig = f;
        (void)rgd_ident_format_ids(f.b, f.len, 7, s, sizeof(s));
        (void)rgd_ident_format_param_ids(f.b, f.len, s, sizeof(s));
        r = rgd_ident_add_ids(f.b, f.len, cap, 6, rgd_ident_sent_ids, 2);
        if (r == 0) {
            CHECK(memcmp(f.b, orig.b, sizeof(f.b)) == 0);
        } else {
            CHECK(r <= cap);
            CHECK(rgd_ident_find_param(f.b, r, 6, NULL, NULL) == 1);
            CHECK(rgd_ident_missing_ids(f.b, r, 6, rgd_ident_sent_ids, 2) == 0);
        }
        if (failures > 20) return;
    }
}

int main(void)
{
    test_constants();
    test_cn_unchanged();
    test_er_appended();
    test_partial();
    test_cap_short();
    test_malformed();
    test_format();
    test_fuzz();
    printf("RGD_IDENTIFY_TEST=%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures > 255 ? 255 : failures;
}
