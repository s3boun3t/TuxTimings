#include "pm_table.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void test_unknown_versions(void)
{
    /* Plausible Granite Ridge values cannot establish an unknown layout. */
    float t[480] = {0};
    t[3] = 80.0f;
    t[11] = 45.0f;
    t[79] = 3000.0f;
    t[83] = 1.2f;
    t[271] = 1.1f;
    t[309] = 0.95f;
    t[317] = 40.0f;
    const uint32_t unknown[] = {0, UINT32_MAX, 0x620106, 0x620206, 0x540105};
    for (size_t i = 0; i < sizeof(unknown) / sizeof(unknown[0]); i++) {
        smu_metrics_t m, unavailable;
        memset(&m, 0xa5, sizeof(m));
        memset(&unavailable, 0, sizeof(unavailable));
        pm_table_read(unknown[i], t, 480, 8, &m);
        assert(memcmp(&m, &unavailable, sizeof(m)) == 0);
    }
}

static void test_unmapped_core_arrays(void)
{
    /* Hawk Point explicitly marks every per-core start as -1. ASan catches
     * any accidental read before this allocation, even if it looks valid. */
    float *t = calloc(480, sizeof(*t));
    assert(t);
    t[89] = 2000.0f;
    t[93] = 1500.0f;
    t[97] = 1500.0f;
    t[101] = 1.2f;
    t[477] = 0.9f;
    smu_metrics_t m;
    pm_table_read(0x4c0009, t, 480, 8, &m);
    assert(m.core_temps_count == 0);
    assert(m.core_voltages_count == 0);
    assert(m.core_clocks_count == 0);
    assert(m.fclk_mhz == 2000.0f && m.uclk_mhz == 1500.0f);
    assert(m.mclk_mhz == 1500.0f && m.vsoc == 1.2f && m.vddp == 0.9f);
    free(t);
}

static void test_known_maps_and_recovery(void)
{
    float t[365] = {0};
    t[3] = 80.0f;
    t[71] = 2400.0f;
    t[75] = 1600.0f;
    t[79] = 3000.0f;
    t[83] = 1.2f;
    t[309] = 0.95f;
    t[333] = 42.0f;
    t[349] = 3.2f;
    smu_metrics_t m;
    pm_table_read(0x620205, t, 365, 16, &m);
    assert(m.mclk_mhz == 3000.0f && m.fclk_mhz == 2400.0f && m.uclk_mhz == 1600.0f);
    assert(m.vsoc == 1.2f && m.ppt_w == 80.0f);
    assert(m.core_voltages_count == 1 && m.core_voltages[0] == 0.95f);
    assert(m.core_temps_count == 16 && m.core_temps_c[0] == 42.0f);
    assert(m.core_clocks_count == 16 && m.core_clocks_ghz[0] == 3.2f);
    pm_table_read(0x620206, t, 365, 16, &m);
    assert(m.mclk_mhz == 0.0f && m.core_temps_count == 0);
    pm_table_read(0x620205, t, 365, 16, &m);
    assert(m.mclk_mhz == 3000.0f && m.core_temps_c[0] == 42.0f);
}

static void test_core_array_bounds(void)
{
    /* Existing 0x620105 map: eight voltages at 301, temperatures at 317,
     * clocks at 325. Allocate exactly the declared count for ASan. */
    const struct { int count, voltages, temperatures, clocks; } cases[] = {
        {0, 0, 0, 0}, {3, 0, 0, 0}, {300, 0, 0, 0}, {301, 0, 0, 0},
        {308, 0, 0, 0}, {309, 8, 0, 0}, {316, 8, 0, 0}, {317, 8, 0, 0},
        {324, 8, 0, 0}, {325, 8, 8, 0}, {332, 8, 8, 0}, {333, 8, 8, 8}
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        float *t = calloc(cases[c].count ? (size_t)cases[c].count : 1, sizeof(*t));
        assert(t);
        for (int i = 0; i < cases[c].count; i++) t[i] = 1.0f;
        smu_metrics_t m;
        pm_table_read(0x620105, t, cases[c].count, 8, &m);
        assert(m.core_voltages_count == cases[c].voltages);
        assert(m.core_temps_count == cases[c].temperatures);
        assert(m.core_clocks_count == cases[c].clocks);
        free(t);
    }
}

static void test_ppt_availability(void)
{
    float t[4] = {0};
    smu_metrics_t m;
    /* PPT zero is a real sample; absence, nonfinite and negative are not. */
    pm_table_read(0x620205, t, 4, 16, &m);
    assert(m.ppt_available && m.ppt_w == 0.0f);
    t[3] = 80.0f;
    pm_table_read(0x620205, t, 4, 16, &m);
    assert(m.ppt_available && m.ppt_w == 80.0f);
    const float invalid[] = {-1.0f, NAN, INFINITY, 401.0f};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        t[3] = invalid[i];
        pm_table_read(0x620205, t, 4, 16, &m);
        assert(!m.ppt_available && m.ppt_w == 0.0f);
    }
    t[3] = 80.0f;
    pm_table_read(0x620205, t, 3, 16, &m);
    assert(!m.ppt_available);
    pm_table_read(0x400005, t, 4, 8, &m); /* Known map, PPT index 5 absent. */
    assert(!m.ppt_available);
    pm_table_read(0x620205, t, 4, 16, &m);
    assert(m.ppt_available && m.ppt_w == 80.0f);
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        if (!strcmp(argv[1], "unknown")) test_unknown_versions();
        else if (!strcmp(argv[1], "unmapped")) test_unmapped_core_arrays();
        else if (!strcmp(argv[1], "ppt")) test_ppt_availability();
        else abort();
        return 0;
    }
    test_unknown_versions();
    test_unmapped_core_arrays();
    test_known_maps_and_recovery();
    test_core_array_bounds();
    test_ppt_availability();
    float t[372] = {0};
    smu_metrics_t m;
    for (int i = 0; i < 8; i++) {
        t[180 + i] = 0.96f + i * 0.001f;
        t[188 + i] = 32.0f + i;
        t[212 + i] = 3.5f + i * 0.01f;
    }
    pm_table_read(0x380905, t, 372, 8, &m);
    assert(m.core_voltages_count == 8);
    assert(m.core_temps_count == 8);
    assert(m.core_clocks_count == 8);
    for (int i = 0; i < 8; i++) {
        assert(m.core_voltages[i] == t[180 + i]);
        assert(m.core_temps_c[i] == t[188 + i]);
        assert(m.core_clocks_ghz[i] == t[212 + i]);
    }
    t[183] = 0.0f;
    pm_table_read(0x380905, t, 372, 8, &m);
    assert(m.core_voltages_count == 8);
    assert(m.core_voltages[3] == 0.0f);
    assert(m.core_voltages[4] == t[184]);
    pm_table_read(0x380905, t, 372, 6, &m);
    assert(m.core_temps_count == 6);
    assert(m.core_temps_c[0] == t[186]); /* Legacy path unchanged. */
    pm_table_read(0x380905, t, 371, 8, &m);
    assert(m.core_temps_count == 6); /* Exact layout guard. */
    /* Previously, voltage-like index 11 was exposed as a temperature.
     * Also reject a temperature-like value: mapping, not range, is the bug.
     */
    const float false_iod[] = {1.0f, 1.2f, 45.0f};
    t[39] = 1.3f;
    t[40] = 1.12f;
    t[44] = 1.1f;
    t[45] = 1.087f;
    t[139] = 0.95f; /* Last entries must survive the named-count change. */
    for (unsigned j = 0; j < sizeof(false_iod) / sizeof(false_iod[0]); j++) {
        t[11] = false_iod[j];
        for (int cores = 6; cores <= 8; cores += 2) {
            pm_table_read(0x380905, t, 372, cores, &m);
            assert(!m.has_iod_hotspot);
            assert(m.iod_hotspot_c == 0.0f);
            assert(m.vcore == t[40]);
            assert(m.vsoc == t[45]);
            assert(m.vddg_ccd == t[139]);
        }
    }
    /* Other versions keep their existing behavior in this scoped fix. */
    pm_table_read(0x380904, t, 372, 6, &m);
    assert(m.has_iod_hotspot);
    assert(m.iod_hotspot_c == 45.0f);
    /* Unknown lengths and adjacent versions retain their previous maps. */
    pm_table_read(0x380905, t, 371, 8, &m);
    assert(m.vcore == t[39]);
    assert(m.vsoc == t[44]);
    pm_table_read(0x380805, t, 372, 8, &m);
    assert(m.vcore == t[39]);
    assert(m.vsoc == t[44]);
    pm_table_read(0x380904, t, 372, 6, &m);
    assert(m.vcore == t[40]);
    assert(m.vsoc == t[44]);
    /* An unavailable telemetry sample must not fall back to a setpoint. */
    const float invalid[] = {0.0f, -1.0f, NAN, INFINITY, 45.0f};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        t[40] = t[45] = invalid[i];
        pm_table_read(0x380905, t, 372, 8, &m);
        assert(m.vcore == 0.0f);
        assert(m.vsoc == 0.0f);
    }
    t[40] = 1.15f;
    t[45] = 1.09f;
    pm_table_read(0x380905, t, 372, 8, &m);
    assert(m.vcore == t[40]);
    assert(m.vsoc == t[45]);
    puts("Vermeer eight-slot decoding tests passed; no hardware access");
    return 0;
}
