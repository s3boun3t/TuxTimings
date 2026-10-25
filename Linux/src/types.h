#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_CORES       16
#define MAX_MODULES     4
#define MAX_FANS        8
#define STR_LEN         256
#define STR_SHORT       64

typedef enum { MEM_UNKNOWN, MEM_DDR4, MEM_DDR5, MEM_LPDDR4, MEM_LPDDR5 } mem_type_t;
typedef enum { RANK_SR, RANK_DR, RANK_QR } mem_rank_t;
/* Decoder path, not certification of the physical memory type (e.g. LPDDR). */
typedef enum { DRAM_LAYOUT_NONE, DRAM_LAYOUT_DDR4, DRAM_LAYOUT_DDR5 } dram_layout_t;

/* Metadata describes the value presented, not hardware freshness. */
typedef enum { VALUE_SOURCE_NONE, VALUE_SOURCE_SMN, VALUE_SOURCE_PM,
               VALUE_SOURCE_SMBIOS, VALUE_SOURCE_MSR_ESTIMATE } value_source_t;
typedef enum { VALUE_UNIT_NONE, VALUE_UNIT_MHZ, VALUE_UNIT_MTS } value_unit_t;
typedef struct {
    bool available;
    value_source_t source;
    value_unit_t unit;
} value_info_t;
typedef enum { MEMORY_TYPE_UNKNOWN, MEMORY_TYPE_CONSISTENT,
               MEMORY_TYPE_INCOMPLETE, MEMORY_TYPE_CONFLICT } memory_type_state_t;

/* Per-register userspace I/O status, shared by the known DRAM layouts. */
typedef enum {
    DRAM_REG_50204 = 1u << 0,  DRAM_REG_50208 = 1u << 1,
    DRAM_REG_5020C = 1u << 2,  DRAM_REG_50210 = 1u << 3,
    DRAM_REG_50214 = 1u << 4,  DRAM_REG_50218 = 1u << 5,
    DRAM_REG_5021C = 1u << 6,  DRAM_REG_50220 = 1u << 7,
    DRAM_REG_50224 = 1u << 8,  DRAM_REG_50228 = 1u << 9,
    DRAM_REG_50230 = 1u << 10, DRAM_REG_50234 = 1u << 11,
    DRAM_REG_50250 = 1u << 12, DRAM_REG_50254 = 1u << 13,
    DRAM_REG_50258 = 1u << 14, DRAM_REG_502A4 = 1u << 15,
    DRAM_REG_RATIO = 1u << 16, DRAM_REG_REFRESH = 1u << 17,
    DRAM_VALUE_RFC = 1u << 18, DRAM_VALUE_RFCSB = 1u << 19
} dram_source_mask_t;

typedef enum {
    DIMM_TEMP_UNASSOCIATED, DIMM_TEMP_IDENTITY_MISSING, DIMM_TEMP_AMBIGUOUS,
    DIMM_TEMP_SPD_UNAVAILABLE, DIMM_TEMP_INCOMPATIBLE, DIMM_TEMP_INVENTORY_INCOMPLETE,
    DIMM_TEMP_ASSOCIATED, DIMM_TEMP_SENSOR_MISSING
} dimm_temp_state_t;

typedef struct {
    char bank_label[STR_LEN];
    char device_locator[STR_LEN];
    char manufacturer[STR_LEN];
    char part_number[STR_LEN];
    char serial_number[STR_LEN];
    uint64_t capacity_bytes;
    uint32_t configured_speed; /* SMBIOS numeric value, unit kept separately */
    value_unit_t configured_speed_unit;
    mem_rank_t rank;
    mem_type_t type; /* SMBIOS Type, independent of CPU codename */
    dimm_temp_state_t temp_state;
    bool temperature_available;
    float temperature_c;
    char temperature_sensor[STR_SHORT];
    /* Derived */
    char slot_label[24];   /* e.g. "A1", "Slot 0" */
    char slot_display[64]; /* e.g. "A1 - 16.0 GiB" */
    char capacity_display[32]; /* e.g. "16.0 GiB" */
} memory_module_t;

typedef struct {
    float frequency;       /* MT/s */
    value_info_t frequency_info;
    mem_type_t type;
    memory_type_state_t type_state; /* SMBIOS consensus, never CPU codename */
    char total_capacity[32];
    char part_number[STR_LEN];
} memory_config_t;

typedef struct {
    char name[STR_LEN];
    char processor_name[STR_LEN];
    char codename[STR_SHORT];
    char smu_version[STR_SHORT];
    char pm_table_version[STR_SHORT];
} cpu_info_t;

typedef struct {
    char motherboard[STR_LEN];
    char bios_version[STR_LEN];
    char bios_date[STR_SHORT];
    char agesa_version[STR_LEN];
    char display_line[1024];
} board_info_t;

typedef struct {
    /* Power */
    float package_power_w; /* reserved — populated but not yet displayed */
    float ppt_w;
    bool ppt_available;
    float package_current_a; /* reserved — populated but not yet displayed */
    /* Voltages */
    float vcore, vsoc, vddp, vddg_ccd, vddg_iod, vdd_misc;
    float cpu_vddio, mem_vdd, mem_vddq, mem_vpp;
    float vid;
    /* Clocks */
    float core_clock_mhz, bclk_mhz; /* reserved */
    value_info_t bclk_info;
    float fclk_mhz, uclk_mhz, mclk_mhz;
    float memory_clock_mhz; /* reserved */
    float core_clocks_ghz[MAX_CORES]; /* reserved */
    int   core_clocks_count; /* reserved */
    /* Temps */
    float cpu_temp_c;
    float core_temps_c[MAX_CORES];
    int   core_temps_count;
    float tdie_c;   bool has_tdie;
    float tctl_c;   bool has_tctl;
    float tccd1_c;  bool has_tccd1;
    float tccd2_c;  bool has_tccd2;
    float iod_hotspot_c; bool has_iod_hotspot;
    /* Per-core */
    float core_voltages[MAX_CORES];
    int   core_voltages_count;
    float core_usage_pct[MAX_CORES];
    int   core_usage_count;
    float core_freq_mhz[MAX_CORES];
    int   core_freq_count;
    /* SPD temps */
    float spd_temps_c[MAX_MODULES];
    bool  spd_temps_available[MAX_MODULES];
    int   spd_temps_count;
    /* Sensor identity, independent of unverified SMBIOS slot ordering. */
    char spd_temp_labels[MAX_MODULES][STR_SHORT];
} smu_metrics_t;

typedef struct {
    /* Primary */
    uint32_t tcl, trcd_rd, trcd_wr, trp, tras, trc;
    /* Secondary */
    uint32_t trrds, trrdl, tfaw, twr, tcwl;
    uint32_t rtp, wtrs, wtrl, rdwr, wrrd;
    uint32_t rdrd_scl, wrwr_scl;
    uint32_t rdrd_sc, rdrd_sd, rdrd_dd;
    uint32_t wrwr_sc, wrwr_sd, wrwr_dd;
    uint32_t refi, wrpre, rdpre;
    /* Tertiary */
    uint32_t trc_page, mod, mod_pda, mrd, mrd_pda;
    uint32_t stag, stag_sb, cke, xp;
    uint32_t phy_wrd, phy_wrl, phy_rdl;
    /* RFC */
    uint32_t rfc, rfc2, rfcsb;
    /* Nanoseconds */
    float trefi_ns, trfc_ns;
    float trfc2_ns, trfcsb_ns; /* reserved — computed but not displayed */
    /* Flags */
    bool gdm_enabled, power_down_enabled;
    char cmd2t[4];
    float data_rate_mts; /* Effective DDR rate, not MCLK MHz. */
    value_info_t data_rate_info;
    uint32_t available; /* dram_source_mask_t; never inferred from the value */
    bool trcd_wr_derived, twr_derived; /* Existing zero substitutions */
    /* DDR4 refresh fields; zero for other memory types. FGR is the raw
     * 0x5012C[18:16] code, not a refresh multiplier or an I/O validity flag.
     * 0: normal; 1/2: fixed 2x/4x; 5/6: on-the-fly 1x/2x or 1x/4x.
     * 3/4/7 are reserved. Keep rfc/trfc_ns as the base tRFC value. */
    uint32_t rfc4, fgr;
    float trfc4_ns;
    dram_layout_t layout;
    /* Userspace I/O availability, not proof of hardware validity or freshness.
     * RFC requires both reads and a nonzero selected register; ratio requires
     * a nonzero decoded ratio. Zero FGR is valid only with fgr_io_ok. */
    bool ddr4_fgr_io_ok, ddr4_rfc_available, ddr4_ratio_available;
} dram_timings_t;

typedef struct {
    char label[16];
    int rpm;
} fan_reading_t;

typedef struct {
    cpu_info_t cpu;
    memory_config_t memory;
    board_info_t board;
    memory_module_t modules[MAX_MODULES];
    int module_count;
    smu_metrics_t metrics;
    dram_timings_t dram;
    fan_reading_t fans[MAX_FANS];
    int fan_count;
} system_summary_t;

#endif /* TYPES_H */
