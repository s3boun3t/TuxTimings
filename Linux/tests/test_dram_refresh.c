/* Link-time stdio stubs: exercise the real decoder without opening sysfs. */
#ifdef NDEBUG
#error "This test suite requires assertions; remove -DNDEBUG"
#endif
#include "dram.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned char token;
static uint32_t address, ratio = 54, refresh, rfc0, rfc1;
static unsigned reads, opened, closed;
static int transaction;
static int fail_open;
enum failure { FAIL_NONE, FAIL_WRITE, FAIL_FLUSH, FAIL_SEEK, FAIL_READ, FAIL_CLOSE };
static enum failure fail;
static uint32_t fail_address;
#define STREAM ((FILE *)&token)
static int fails(enum failure stage) { return fail == stage && address == fail_address; }
FILE *__wrap_fopen(const char *path, const char *mode)
{
    assert(strcmp(path, "/sys/kernel/ryzen_smu_drv/smn") == 0);
    assert(strcmp(mode, "r+b") == 0);
    if (fail_open) return NULL;
    opened++;
    transaction = 0;
    return STREAM;
}
size_t __wrap_fwrite(const void *ptr, size_t size, size_t n, FILE *f)
{
    /* This explicitly rejects eight-byte target-register writes. */
    assert(f == STREAM && size == 1 && n == 4 && transaction == 0);
    const unsigned char *p = ptr;
    address = (uint32_t)p[0] | (uint32_t)p[1] << 8 |
              (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
    transaction = fails(FAIL_WRITE) ? -1 : 1;
    return fails(FAIL_WRITE) ? 3 : n;
}
int __wrap_fflush(FILE *f)
{
    assert(f == STREAM && transaction == 1);
    transaction = fails(FAIL_FLUSH) ? -1 : 2;
    return fails(FAIL_FLUSH) ? EOF : 0;
}
int __wrap_fseek(FILE *f, long offset, int whence)
{
    assert(f == STREAM && transaction == 2 && offset == 0 && whence == SEEK_SET);
    transaction = fails(FAIL_SEEK) ? -1 : 3;
    return fails(FAIL_SEEK) ? -1 : 0;
}
void __wrap_rewind(FILE *f) { (void)f; assert(!"use checked fseek, not rewind"); }
size_t __wrap_fread(void *ptr, size_t size, size_t n, FILE *f)
{
    assert(f == STREAM && size == 1 && n == 4 && transaction == 3);
    uint32_t v = 0;
    switch (address) {
    case 0x50200: v = ratio; break;
    case 0x5012C: v = refresh; break;
    case 0x50260: v = rfc0; break;
    case 0x50264: v = rfc1; break;
    case 0x50230: v = 14040; break;
    }
    unsigned char *p = ptr;
    for (unsigned i = 0; i < 4; i++) p[i] = (unsigned char)(v >> (8 * i));
    reads++;
    transaction = fails(FAIL_READ) ? -1 : 4;
    return fails(FAIL_READ) ? 3 : n;
}
size_t __wrap___fread_chk(void *ptr, size_t len, size_t size, size_t n, FILE *f)
{
    assert(size * n <= len);
    return __wrap_fread(ptr, size, n, f);
}
int __wrap_fclose(FILE *f)
{
    assert(f == STREAM);
    closed++;
    return fails(FAIL_CLOSE) ? EOF : 0;
}
static void near(float actual, float expected)
{
    if (fabsf(actual - expected) >= 0.01f)
        fprintf(stderr, "actual=%f expected=%f\n", actual, expected);
    assert(fabsf(actual - expected) < 0.01f);
}
static void read_ddr4(dram_timings_t *d)
{
    dram_read_timings(12, d);
    assert(opened == closed);
    assert(d->layout == DRAM_LAYOUT_DDR4);
}
static void check(uint32_t a, uint32_t b, uint32_t expected)
{
    dram_timings_t d;
    rfc0 = a; rfc1 = b;
    read_ddr4(&d);
    assert(d.rfc == (expected & 2047));
    assert(d.rfc2 == ((expected >> 11) & 2047));
    assert(d.rfc4 == (expected >> 22));
    assert(d.ddr4_rfc_available == (expected != 0));
}
int main(void)
{
    const uint32_t value = 360u | (260u << 11) | (160u << 22);
    const uint32_t def = 0x21060138;
    check(value, value, value);
    check(def, value, value);
    check(value, def, value);
    check(value, value + 1, value);
    check(def, def, def);
    check(0, 0, 0);
    check(0, value, 0); /* Preserve selection even for a zero register. */
    check(UINT32_MAX, UINT32_MAX, UINT32_MAX);
    rfc0 = rfc1 = value;
    dram_timings_t d;
    const char *labels[] = {"Normal 1×", "Fixed 2×", "Fixed 4×", "Reserved (3)",
                            "Reserved (4)", "On-the-fly 1×/2×", "On-the-fly 1×/4×",
                            "Reserved (7)"};
    for (unsigned fgr = 0; fgr < 8; fgr++) {
        refresh = (fgr << 16) | (1u << 28) | (1u << 19) | 0xffff;
        read_ddr4(&d);
        assert(d.fgr == fgr && d.power_down_enabled && d.ddr4_fgr_io_ok);
        assert(strcmp(dram_fgr_label(d.fgr), labels[fgr]) == 0);
        assert(d.rfc == 360 && d.rfc2 == 260 && d.rfc4 == 160);
        assert(d.ddr4_ratio_available && d.ddr4_rfc_available);
        near(d.trfc_ns, 200); near(d.trfc2_ns, 260.0f / 1.8f);
        near(d.trfc4_ns, 160.0f / 1.8f); near(d.trefi_ns, 7800);
    }
    assert(strcmp(dram_fgr_label(8), "Unknown") == 0);
    ratio = 24; /* DDR4-1600: 360 clocks = 450 ns, not 225 ns. */
    read_ddr4(&d); near(d.trfc_ns, 450); near(d.trfc4_ns, 200);
    ratio = 0;
    read_ddr4(&d);
    assert(!d.ddr4_ratio_available && d.ddr4_rfc_available);
    assert(d.trfc_ns == 0 && d.trfc2_ns == 0 && d.trfc4_ns == 0 && d.trefi_ns == 0);
    const int ddr4[] = {2,3,7,8,14,16,17,4,9,10,12,18,19};
    for (unsigned i = 0; i < sizeof(ddr4)/sizeof(*ddr4); i++) {
        dram_read_timings(ddr4[i], &d);
        assert(d.layout == DRAM_LAYOUT_DDR4 && d.rfc4 == 160 && d.fgr == 7);
    }
    ratio = 3000; rfc0 = 480u | (260u << 16);
    const int ddr5[] = {20,21,22,23,24};
    for (unsigned i = 0; i < sizeof(ddr5)/sizeof(*ddr5); i++) {
        dram_read_timings(ddr5[i], &d);
        assert(d.layout == DRAM_LAYOUT_DDR5 && d.rfc == 480 && d.rfc2 == 260);
        near(d.trfc_ns, 160);
        assert(d.rfc4 == 0 && d.fgr == 0 && d.trfc4_ns == 0);
        assert(!d.ddr4_fgr_io_ok && !d.ddr4_rfc_available && !d.ddr4_ratio_available);
    }
    unsigned before = reads;
    dram_read_timings(-1, &d);
    assert(reads == before && d.rfc4 == 0 && d.layout == DRAM_LAYOUT_NONE);

    /* Recorded Vermeer capture, with unchanged RFC1 selection. */
    ratio = 54; refresh = 0x01100118; rfc0 = def; rfc1 = 0x480EA276;
    read_ddr4(&d);
    assert(d.rfc == 630 && d.rfc2 == 468 && d.rfc4 == 288 && d.fgr == 0);
    near(d.trfc_ns, 350); near(d.trfc2_ns, 260); near(d.trfc4_ns, 160);
    assert(d.ddr4_fgr_io_ok && d.ddr4_rfc_available && d.ddr4_ratio_available);

    fail_open = 1;
    read_ddr4(&d);
    assert(d.rfc4 == 0 && d.fgr == 0 && d.trfc4_ns == 0);
    assert(!d.ddr4_fgr_io_ok && !d.ddr4_rfc_available && !d.ddr4_ratio_available);
    fail_open = 0;
    for (fail = FAIL_WRITE; fail <= FAIL_CLOSE; fail++) {
        fail_address = 0x5012C;
        refresh = 6u << 16;
        read_ddr4(&d);
        assert(!d.ddr4_fgr_io_ok && d.fgr == 0);
        assert(d.ddr4_rfc_available && d.ddr4_ratio_available);
        fail_address = 0x50200;
        read_ddr4(&d);
        assert(!d.ddr4_ratio_available && d.ddr4_rfc_available && d.ddr4_fgr_io_ok);
        assert(d.rfc4 == 288 && d.trfc4_ns == 0);
        fail_address = 0x50260;
        read_ddr4(&d);
        assert(!d.ddr4_rfc_available && d.rfc == 0); /* No new fallback to RFC1. */
        fail_address = 0x50264;
        rfc0 = 0x480EA276; rfc1 = def;
        read_ddr4(&d);
        assert(!d.ddr4_rfc_available && d.rfc == 630); /* Same selection, unavailable UI. */
        rfc0 = def; rfc1 = 0x480EA276;
    }
    fail = FAIL_NONE; refresh = 0; /* A successfully read zero is normal FGR. */
    read_ddr4(&d);
    assert(d.ddr4_fgr_io_ok && d.fgr == 0 && d.ddr4_rfc_available);
    assert(d.rfc4 == 288 && d.ddr4_ratio_available);
    near(d.trfc4_ns, 160);
    puts("DDR4 refresh I/O, FGR and decoder-route tests passed; no hardware access");
    return 0;
}
