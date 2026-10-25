/* Pure configuration/frequency policy: no backend, commands or hardware. */
#include "memory_config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    memory_module_t modules[2] = {
        {.type=MEM_DDR4, .configured_speed=3600, .configured_speed_unit=VALUE_UNIT_MTS},
        {.type=MEM_DDR4, .configured_speed=3600, .configured_speed_unit=VALUE_UNIT_MTS}
    };
    dram_timings_t d = {.layout=DRAM_LAYOUT_DDR4, .data_rate_mts=3600,
        .data_rate_info={true,VALUE_SOURCE_SMN,VALUE_UNIT_MTS}};
    memory_config_t c;
    memory_config_resolve(&c, modules, 2, true, &d, 1800);
    assert(c.type == MEM_DDR4 && c.type_state == MEMORY_TYPE_CONSISTENT);
    assert(c.frequency == 3600 && c.frequency_info.available);
    assert(c.frequency_info.source == VALUE_SOURCE_SMN);
    /* A CPU decoder route must not certify the physical memory type. */
    memory_config_resolve(&c, NULL, 0, false, &d, 1800);
    assert(c.type == MEM_UNKNOWN && c.type_state == MEMORY_TYPE_INCOMPLETE);
    assert(c.frequency == 3600 && c.frequency_info.source == VALUE_SOURCE_SMN);
    for (int codename=20; codename<=24; codename++) {
        assert(dram_layout_for_codename(codename) == DRAM_LAYOUT_DDR5);
        d.layout = dram_layout_for_codename(codename);
        d.data_rate_mts = 6000;
        modules[0].type = modules[1].type = MEM_DDR5;
        memory_config_resolve(&c, modules, 2, true, &d, 3000);
        assert(c.type == MEM_DDR5 && c.frequency == 6000);
    }
    modules[1].type = MEM_DDR4; /* Contradiction rejects the UMC hint. */
    memory_config_resolve(&c, modules, 2, true, &d, 3000);
    assert(c.type == MEM_UNKNOWN && c.type_state == MEMORY_TYPE_CONFLICT);
    assert(!c.frequency_info.available);
    modules[1].type = MEM_UNKNOWN;
    memory_config_resolve(&c, modules, 2, true, &d, 3000);
    assert(c.type == MEM_UNKNOWN && c.type_state == MEMORY_TYPE_INCOMPLETE);
    modules[1].type = MEM_DDR5;
    memory_config_resolve(&c, modules, 2, false, &d, 3000);
    assert(c.type_state == MEMORY_TYPE_INCOMPLETE);
    /* SMBIOS configured speed is not a current UMC reading. */
    d.data_rate_info.available = false;
    memory_config_resolve(&c, modules, 2, true, &d, 3000);
    assert(c.frequency == 3600 && c.frequency_info.source == VALUE_SOURCE_SMBIOS);
    modules[1].configured_speed = 3200;
    memory_config_resolve(&c, modules, 2, true, &d, 3000);
    assert(c.frequency == 6000 && c.frequency_info.source == VALUE_SOURCE_PM);
    modules[0].configured_speed = modules[1].configured_speed = 0;
    memory_config_resolve(&c, modules, 2, true, &d, 3000);
    assert(c.frequency == 6000 && c.frequency_info.unit == VALUE_UNIT_MTS);
    memory_config_resolve(&c, modules, 2, true, &d, 0);
    assert(!c.frequency_info.available && c.frequency == 0);
    memory_config_resolve(&c, modules, 2, true, &d, NAN);
    assert(!c.frequency_info.available);
    memory_config_resolve(&c, modules, 2, true, &d, INFINITY);
    assert(!c.frequency_info.available);
    /* Legacy dmidecode MHz labels do not establish a clock-to-rate relation. */
    modules[0].type = modules[1].type = MEM_DDR4;
    modules[0].configured_speed = modules[1].configured_speed = 1800;
    modules[0].configured_speed_unit = modules[1].configured_speed_unit = VALUE_UNIT_MHZ;
    memory_config_resolve(&c, modules, 2, true, &d, 0);
    assert(!c.frequency_info.available);
    /* A known SMBIOS type incompatible with the ratio route rejects that
     * ratio; independent configured/PM sources remain usable. */
    d.layout = DRAM_LAYOUT_DDR5; d.data_rate_mts = 6000;
    d.data_rate_info.available = true;
    memory_config_resolve(&c, modules, 2, true, &d, 1800);
    assert(c.type == MEM_DDR4 && c.frequency == 3600);
    assert(c.frequency_info.source == VALUE_SOURCE_PM);
    modules[0].configured_speed = modules[1].configured_speed = 3200;
    modules[0].configured_speed_unit = modules[1].configured_speed_unit = VALUE_UNIT_MTS;
    memory_config_resolve(&c, modules, 2, true, &d, 1800);
    assert(c.frequency == 3200 && c.frequency_info.source == VALUE_SOURCE_SMBIOS);
    d.data_rate_info.available = false;
    modules[0].configured_speed = modules[1].configured_speed = 1800;
    modules[0].configured_speed_unit = modules[1].configured_speed_unit = VALUE_UNIT_MHZ;
    for (int t=MEM_LPDDR4; t<=MEM_LPDDR5; t++) {
        modules[0].type = modules[1].type = t;
        memory_config_resolve(&c, modules, 2, true, &d, 1800);
        assert(c.type == (mem_type_t)t && !c.frequency_info.available);
        modules[0].configured_speed_unit = modules[1].configured_speed_unit = VALUE_UNIT_MTS;
        memory_config_resolve(&c, modules, 2, true, &d, 1800);
        assert(c.frequency == 1800 && c.frequency_info.source == VALUE_SOURCE_SMBIOS);
        modules[0].configured_speed_unit = modules[1].configured_speed_unit = VALUE_UNIT_MHZ;
    }
    modules[0].type = modules[1].type = MEM_UNKNOWN;
    d.layout = DRAM_LAYOUT_NONE; d.data_rate_info.available = true;
    memory_config_resolve(&c, modules, 2, true, &d, 1800);
    assert(c.type == MEM_UNKNOWN && !c.frequency_info.available);
    memory_config_resolve(&c, NULL, 0, true, &d, 1800);
    assert(c.type_state == MEMORY_TYPE_UNKNOWN);
    puts("Memory type, frequency units and provenance passed; synthetic only");
    return 0;
}
