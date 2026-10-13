/*
 * Host test for the whole Identify patcher (rgd_module_def.on_identify):
 * message-id registration + 0x001E component, on top of the real framework and
 * routeguidance sources.  Run by Tools/test_rgd_identify.sh.
 *
 * Not covered here: rgd_resize_identify_buffer.  It reads Cinemo's 32-bit
 * NmeArray {base@0, len@4, cap@8}, which a 64-bit host cannot model, so these
 * cases pass ctx._priv = NULL and give the patcher enough capacity, except the
 * one that checks a short buffer is left untouched.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/framework/hook_framework.h"
#include "../src/routeguidance/rgd_hook.h"
#include "../src/routeguidance/rgd_identify.h"
#include "../src/routeguidance/rgd_tlv.h"

/* main.c's module table, reduced to the module under test. */
const hook_module_def_t* const hook_module_table[] = { &rgd_module_def };
const size_t hook_module_table_count = 1;

static int failures;
#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
    failures++; } } while (0)

typedef struct { uint8_t b[4096]; size_t len; } frame_t;

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static unsigned get16(const uint8_t *p) { return (unsigned)(p[0] << 8 | p[1]); }

static void f_begin(frame_t *f)
{
    memset(f, 0, sizeof(*f));
    f->b[0] = 0x40; f->b[1] = 0x40;
    put16(f->b + 4, 0x1d01);
    f->len = 6;
}
static void f_raw(frame_t *f, const uint8_t *p, size_t n) { memcpy(f->b + f->len, p, n); f->len += n; }
static void f_param(frame_t *f, unsigned id, const uint8_t *data, size_t n)
{
    put16(f->b + f->len, (unsigned)(4 + n));
    put16(f->b + f->len + 2, id);
    if (n) memcpy(f->b + f->len + 4, data, n);
    f->len += 4 + n;
}
static void f_list(frame_t *f, unsigned id, const unsigned *ids, size_t n)
{
    uint8_t d[128];
    for (size_t i = 0; i < n; i++) put16(d + 2 * i, ids[i]);
    f_param(f, id, d, 2 * n);
}
static void f_end(frame_t *f) { put16(f->b + 2, (unsigned)f->len); }

static const unsigned er_sent[] = { 0x5000, 0x5002, 0xae00, 0xae02, 0x4e01 };
static const unsigned er_recv[] = { 0x5001, 0xae01, 0xfffe, 0x4e09 };
static const unsigned cn_sent[] = { 0x5000, 0x5002, 0x5200, 0x5203, 0xae00 };
static const unsigned cn_recv[] = { 0x5001, 0x5201, 0x5202, 0x5204, 0xae01 };

static uint16_t default_id;      /* the module's id before any stale component */

enum comp_pos { COMP_NONE, COMP_AFTER_LISTS, COMP_BEFORE_LISTS };

/* A component with id 0x0042 inside (stale content), as a prior patch left it.
 * Its size differs from a fresh one, so rewriting it moves what follows. */
static void f_stale_component(frame_t *f)
{
    uint8_t stale[16];
    memset(stale, 0xcc, sizeof(stale));
    put16(stale, 6); put16(stale + 2, 0); put16(stale + 4, 0x0042);
    f_param(f, 0x1e, stale, sizeof(stale));
}

/* Identify with name, lists, power and a USB transport group, and optionally
 * a stale 0x001E component before or after the lists. */
static void build(frame_t *f, int cn, enum comp_pos comp)
{
    static const uint8_t name[] = "MMI\0";
    static const uint8_t usb[] = { 0x00, 0x06, 0x00, 0x00, 0x00, 0x02 };
    f_begin(f);
    f_param(f, 0, name, sizeof(name));
    if (comp == COMP_BEFORE_LISTS) f_stale_component(f);
    if (cn) { f_list(f, 6, cn_sent, 5); f_list(f, 7, cn_recv, 5); }
    else    { f_list(f, 6, er_sent, 5); f_list(f, 7, er_recv, 4); }
    if (comp == COMP_AFTER_LISTS) f_stale_component(f);
    f_param(f, 8, (const uint8_t *)"\x02", 1);
    f_param(f, 0x0f, usb, sizeof(usb));
    f_end(f);
}

static size_t run_patcher(frame_t *f, size_t cap)
{
    hook_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    size_t n = rgd_module_def.on_identify(&ctx, f->b, f->len, cap);
    /* The async log queue (64 entries) drops the oldest line when a producer
     * outruns the writer; one Identify per connection never does, a tight
     * test loop can.  Let the writer drain between runs. */
    usleep(50000);
    return n;
}

static int count_param(const frame_t *f, size_t len, unsigned id)
{
    int n = 0;
    for (size_t off = 6; off + 4 <= len; off += get16(f->b + off))
        if (get16(f->b + off + 2) == id) n++;
    return n;
}

/* Params other than 6, 7, 0x1e: identical bytes, identical order. */
static int others_equal(const uint8_t *a, size_t la, const uint8_t *b, size_t lb)
{
    size_t oa = 6, ob = 6;
    for (;;) {
#define SKIP(p, o, l) while ((o) < (l) && (get16((p) + (o) + 2) == 6 || get16((p) + (o) + 2) == 7 || \
                                          get16((p) + (o) + 2) == 0x1e)) (o) += get16((p) + (o))
        SKIP(a, oa, la);
        SKIP(b, ob, lb);
        if (oa >= la || ob >= lb) return oa >= la && ob >= lb;
        if (get16(a + oa) != get16(b + ob) || memcmp(a + oa, b + ob, get16(a + oa))) return 0;
        oa += get16(a + oa);
        ob += get16(b + ob);
    }
}

static size_t comp_len_for(uint16_t id)
{
    uint8_t c[256];
    return rgd_build_component_tlv(c, sizeof(c), id);
}

static void check_component(const frame_t *f, size_t len, uint16_t id)
{
    uint8_t want[256];
    size_t want_len = rgd_build_component_tlv(want, sizeof(want), id);
    size_t off, plen;
    CHECK(rgd_ident_find_param(f->b, len, 0x1e, &off, &plen) == 1);
    CHECK(count_param(f, len, 0x1e) == 1);
    CHECK(plen == want_len && memcmp(f->b + off, want, want_len) == 0);
}

static void test_er(enum comp_pos comp)
{
    int with_component = (comp != COMP_NONE);
    frame_t f, orig;
    build(&f, 0, comp);
    orig = f;
    size_t n = run_patcher(&f, sizeof(f.b));
    CHECK(n > orig.len);
    CHECK(get16(f.b + 2) == n);
    CHECK(rgd_ident_missing_ids(f.b, n, 6, rgd_ident_sent_ids, RGD_IDENT_SENT_COUNT) == 0);
    CHECK(rgd_ident_missing_ids(f.b, n, 7, rgd_ident_received_ids, RGD_IDENT_RECEIVED_COUNT) == 0);
    uint16_t id = with_component ? 0x0042 : default_id;
    check_component(&f, n, id);
    CHECK(others_equal(orig.b, orig.len, f.b, n));
    CHECK(n == orig.len + 10 + comp_len_for(id) - (with_component ? 20u : 0u));
    {   /* lists keep their original entries first */
        size_t off, plen;
        rgd_ident_find_param(f.b, n, 6, &off, &plen);
        for (size_t i = 0; i < 5; i++) CHECK(get16(f.b + off + 4 + 2 * i) == er_sent[i]);
        rgd_ident_find_param(f.b, n, 7, &off, &plen);
        for (size_t i = 0; i < 4; i++) CHECK(get16(f.b + off + 4 + 2 * i) == er_recv[i]);
    }
}

static void test_cn(void)
{
    frame_t f, orig;
    size_t off_o, plen_o, off_n, plen_n;
    build(&f, 1, COMP_NONE);
    orig = f;
    size_t n = run_patcher(&f, sizeof(f.b));
    CHECK(n == orig.len + comp_len_for(default_id));   /* only the component */
    rgd_ident_find_param(orig.b, orig.len, 6, &off_o, &plen_o);
    rgd_ident_find_param(f.b, n, 6, &off_n, &plen_n);
    CHECK(plen_o == plen_n && off_o == off_n && memcmp(orig.b + off_o, f.b + off_n, plen_o) == 0);
    rgd_ident_find_param(orig.b, orig.len, 7, &off_o, &plen_o);
    rgd_ident_find_param(f.b, n, 7, &off_n, &plen_n);
    CHECK(plen_o == plen_n && memcmp(orig.b + off_o, f.b + off_n, plen_o) == 0);
    CHECK(memcmp(orig.b + 4, f.b + 4, orig.len - 4) == 0);   /* prefix untouched */
    check_component(&f, n, default_id);
}

static void test_short_buffer_untouched(void)
{
    frame_t f, orig;
    build(&f, 0, COMP_NONE);
    orig = f;
    /* ctx._priv is NULL, so the patcher cannot grow: it must skip entirely. */
    size_t n = run_patcher(&f, f.len + 4);
    CHECK(n == orig.len);
    CHECK(memcmp(f.b, orig.b, sizeof(f.b)) == 0);
}

static void test_malformed_lists(void)
{
    frame_t f, orig;
    static const uint8_t junk[3] = { 0x52, 0x00, 0x01 };
    f_begin(&f);
    f_param(&f, 0, (const uint8_t *)"MMI\0", 4);
    f_param(&f, 6, junk, sizeof(junk));               /* odd list length */
    f_list(&f, 7, er_recv, 4);
    f_end(&f);
    orig = f;
    size_t n = run_patcher(&f, sizeof(f.b));
    /* Lists are left alone; the component is still appended (legacy behaviour). */
    CHECK(n == orig.len + comp_len_for(default_id));
    CHECK(memcmp(f.b + 4, orig.b + 4, orig.len - 4) == 0);
    check_component(&f, n, default_id);
}

int main(int argc, char** argv)
{
    /* Log into the caller's scratch file at INFO (what diagnostics.enabled
     * gives on the car) so every new log statement is formatted under ASan;
     * never /tmp. */
    log_config_t lc = LOG_CONFIG_DEFAULT;
    lc.log_path = argc > 1 ? argv[1] : NULL;
    lc.min_level = LOG_LEVEL_INFO;
    if (log_init(&lc) != HOOK_OK) { fprintf(stderr, "log_init failed\n"); return 1; }

    /* Learn the module's default component id from one unpatched run. */
    {
        frame_t f;
        size_t off, plen;
        build(&f, 0, COMP_NONE);
        run_patcher(&f, sizeof(f.b));
        if (rgd_ident_find_param(f.b, get16(f.b + 2), 0x1e, &off, &plen) != 1) {
            fprintf(stderr, "first run produced no component\n");
            return 1;
        }
        default_id = rgd_extract_component_id(f.b + off, plen);
    }
    test_er(COMP_NONE);
    test_cn();
    test_short_buffer_untouched();
    test_malformed_lists();
    /* Last: an existing component rewrites the module's id. */
    test_er(COMP_AFTER_LISTS);
    test_er(COMP_BEFORE_LISTS);
    log_shutdown();
    printf("RGD_PATCHER_TEST=%s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures > 255 ? 255 : failures;
}
