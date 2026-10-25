#include "pm_table.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

int main(void)
{
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
