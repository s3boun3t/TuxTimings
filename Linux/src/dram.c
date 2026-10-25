#include "dram.h"
#include "util.h"
#include <string.h>
#include <stdio.h>

#define SMN_PATH "/sys/kernel/ryzen_smu_drv/smn"

/* This reports userspace I/O success only. ryzen_smu 0.1.7 has a shared
 * result buffer and does not propagate every hardware failure to syscalls. */
static bool try_read_smn(uint32_t address, uint32_t *value)
{
    *value = 0;
    FILE *f = fopen(SMN_PATH, "r+b");
    if (!f) return false;
    /* Exactly four address bytes request a read; eight bytes would write
     * a target register. Never retry a partial submission. */
    uint8_t buf[4];
    buf[0] = (uint8_t)(address);
    buf[1] = (uint8_t)(address >> 8);
    buf[2] = (uint8_t)(address >> 16);
    buf[3] = (uint8_t)(address >> 24);
    bool ok = fwrite(buf, 1, sizeof(buf), f) == sizeof(buf);
    if (ok) ok = fflush(f) == 0;
    if (ok) ok = fseek(f, 0, SEEK_SET) == 0;
    if (ok) ok = fread(buf, 1, sizeof(buf), f) == sizeof(buf);
    if (fclose(f) != 0) ok = false;
    if (ok)
        *value = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                 ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    return ok;
}

static uint32_t read_register(uint32_t address, uint32_t mask, dram_timings_t *d)
{
    uint32_t value;
    if (try_read_smn(address, &value)) d->available |= mask;
    return value;
}

const char *dram_fgr_label(uint32_t code)
{
    switch (code) {
    case 0: return "Normal 1×";
    case 1: return "Fixed 2×";
    case 2: return "Fixed 4×";
    case 3: return "Reserved (3)";
    case 4: return "Reserved (4)";
    case 5: return "On-the-fly 1×/2×";
    case 6: return "On-the-fly 1×/4×";
    case 7: return "Reserved (7)";
    default: return "Unknown";
    }
}

/* data_rate_mts is effective DDR MT/s; the clock period is 2000/rate ns. */
static void convert_timings(dram_timings_t *d)
{
    if (!d->data_rate_info.available) return;
    const float period_ns = 2000.0f / d->data_rate_mts;
    if (dram_has(d, DRAM_REG_50230)) d->trefi_ns = d->refi * period_ns;
    if (dram_has(d, DRAM_VALUE_RFC)) {
        d->trfc_ns = d->rfc * period_ns;
        d->trfc2_ns = d->rfc2 * period_ns;
        d->trfc4_ns = d->rfc4 * period_ns;
    }
    if (dram_has(d, DRAM_VALUE_RFCSB)) d->trfcsb_ns = d->rfcsb * period_ns;
}

/* Common timing extraction from SMN registers — shared between DDR4 and DDR5 */
static void read_common_timings(uint32_t offset, dram_timings_t *d)
{
    uint32_t reg50204 = read_register(offset | 0x50204, DRAM_REG_50204, d);
    uint32_t reg50208 = read_register(offset | 0x50208, DRAM_REG_50208, d);
    uint32_t reg5020C = read_register(offset | 0x5020C, DRAM_REG_5020C, d);
    uint32_t reg50210 = read_register(offset | 0x50210, DRAM_REG_50210, d);
    uint32_t reg50214 = read_register(offset | 0x50214, DRAM_REG_50214, d);
    uint32_t reg50218 = read_register(offset | 0x50218, DRAM_REG_50218, d);
    uint32_t reg5021C = read_register(offset | 0x5021C, DRAM_REG_5021C, d);
    uint32_t reg50220 = read_register(offset | 0x50220, DRAM_REG_50220, d);
    uint32_t reg50224 = read_register(offset | 0x50224, DRAM_REG_50224, d);
    uint32_t reg50228 = read_register(offset | 0x50228, DRAM_REG_50228, d);
    uint32_t reg50230 = read_register(offset | 0x50230, DRAM_REG_50230, d);
    uint32_t reg50234 = read_register(offset | 0x50234, DRAM_REG_50234, d);
    uint32_t reg50250 = read_register(offset | 0x50250, DRAM_REG_50250, d);
    uint32_t reg50254 = read_register(offset | 0x50254, DRAM_REG_50254, d);
    uint32_t reg50258 = read_register(offset | 0x50258, DRAM_REG_50258, d);
    uint32_t reg502A4 = read_register(offset | 0x502A4, DRAM_REG_502A4, d);

    d->tcl     = bit_slice(reg50204, 5, 0);
    d->trcd_rd = bit_slice(reg50204, 21, 16);
    d->trcd_wr = bit_slice(reg50204, 29, 24);
    if (d->trcd_wr == 0 && dram_has(d, DRAM_REG_50204)) {
        d->trcd_wr = d->trcd_rd;
        d->trcd_wr_derived = true;
    }
    d->tras    = bit_slice(reg50204, 14, 8);
    d->trp     = bit_slice(reg50208, 21, 16);
    d->trc     = bit_slice(reg50208, 7, 0);

    d->trrds   = bit_slice(reg5020C, 4, 0);
    d->trrdl   = bit_slice(reg5020C, 12, 8);
    d->tfaw    = bit_slice(reg50210, 7, 0);
    d->rtp     = bit_slice(reg5020C, 28, 24);
    d->wtrs    = bit_slice(reg50214, 12, 8);
    d->wtrl    = bit_slice(reg50214, 22, 16);
    d->tcwl    = bit_slice(reg50214, 5, 0);
    d->twr     = bit_slice(reg50218, 7, 0);
    if (d->twr == 0 && dram_has(d, DRAM_REG_50218)) {
        d->twr = d->wtrs;
        d->twr_derived = true;
    }

    d->trc_page = bit_slice(reg5021C, 31, 20);

    d->rdrd_scl = bit_slice(reg50220, 29, 24);
    d->rdrd_sc  = bit_slice(reg50220, 19, 16);
    d->rdrd_sd  = bit_slice(reg50220, 11, 8);
    d->rdrd_dd  = bit_slice(reg50220, 3, 0);

    d->wrwr_scl = bit_slice(reg50224, 29, 24);
    d->wrwr_sc  = bit_slice(reg50224, 19, 16);
    d->wrwr_sd  = bit_slice(reg50224, 11, 8);
    d->wrwr_dd  = bit_slice(reg50224, 3, 0);

    d->rdwr = bit_slice(reg50228, 13, 8);
    d->wrrd = bit_slice(reg50228, 3, 0);
    d->refi = bit_slice(reg50230, 15, 0);

    d->mod_pda = bit_slice(reg50234, 29, 24);
    d->mrd_pda = bit_slice(reg50234, 21, 16);
    d->mod     = bit_slice(reg50234, 13, 8);
    d->mrd     = bit_slice(reg50234, 5, 0);

    d->stag    = bit_slice(reg50250, 26, 16);
    d->stag_sb = bit_slice(reg50250, 8, 0);

    d->cke = bit_slice(reg50254, 28, 24);
    d->xp  = bit_slice(reg50254, 5, 0);

    d->phy_wrd = bit_slice(reg50258, 26, 24);
    d->phy_rdl = bit_slice(reg50258, 23, 16);
    d->phy_wrl = bit_slice(reg50258, 15, 8);

    d->wrpre = bit_slice(reg502A4, 10, 8);
    d->rdpre = bit_slice(reg502A4, 2, 0);
}

static void read_ddr5_timings(dram_timings_t *d)
{
    const uint32_t offset = 0; /* UMC0 */

    /* Ratio -> frequency */
    uint32_t ratio_reg = read_register(offset | 0x50200, DRAM_REG_RATIO, d);
    float ratio = bit_slice(ratio_reg, 15, 0) / 100.0f;
    float mem_freq = ratio * 200.0f;
    d->data_rate_mts = mem_freq;
    d->data_rate_info = (value_info_t){
        dram_has(d, DRAM_REG_RATIO) && mem_freq > 0.0f, VALUE_SOURCE_SMN, VALUE_UNIT_MTS
    };

    /* GDM, Cmd2T, PowerDown */
    d->gdm_enabled = bit_slice(ratio_reg, 18, 18) == 1;
    uint32_t cmd2t_bit = bit_slice(ratio_reg, 17, 17);
    if (dram_has(d, DRAM_REG_RATIO))
        snprintf(d->cmd2t, sizeof(d->cmd2t), "%s", cmd2t_bit ? "2T" : "1T");
    uint32_t refresh_reg = read_register(offset | 0x5012C, DRAM_REG_REFRESH, d);
    d->power_down_enabled = bit_slice(refresh_reg, 28, 28) == 1;

    read_common_timings(offset, d);

    /* RFC - DDR5: choose first non-default from 4 registers */
    uint32_t trfc_regs[4];
    bool rfc_ok = true;
    for (unsigned i = 0; i < 4; i++) {
        bool ok = try_read_smn(offset | (0x50260 + 4 * i), &trfc_regs[i]);
        rfc_ok = rfc_ok && ok;
    }
    uint32_t trfc_reg = 0;
    for (int i = 0; i < 4; i++) {
        if (trfc_regs[i] != 0x00C00138) { trfc_reg = trfc_regs[i]; break; }
    }
    if (rfc_ok && trfc_reg) d->available |= DRAM_VALUE_RFC;
    if (trfc_reg) {
        d->rfc  = bit_slice(trfc_reg, 15, 0);
        d->rfc2 = bit_slice(trfc_reg, 31, 16);
    }

    /* RFCsb */
    uint32_t rfcsb_regs[4];
    bool rfcsb_ok = true;
    for (unsigned i = 0; i < 4; i++) {
        uint32_t value;
        bool ok = try_read_smn(offset | (0x502C0 + 4 * i), &value);
        rfcsb_ok = rfcsb_ok && ok;
        rfcsb_regs[i] = bit_slice(value, 10, 0);
    }
    for (unsigned i = 0; i < 4; i++) {
        if (rfcsb_regs[i] != 0) { d->rfcsb = rfcsb_regs[i]; break; }
    }
    if (rfcsb_ok && d->rfcsb) d->available |= DRAM_VALUE_RFCSB;
    convert_timings(d);
}

static void read_ddr4_timings(dram_timings_t *d)
{
    const uint32_t offset = 0; /* UMC0 */

    /*
     * DDR4 frequency ratio / flags (aligned with ZenStates-Core):
     *   - ratio: bits 0..7, scaled by /3.0, then *200 -> effective MT/s
     *   - Cmd2T: bit 10
     *   - GDM:   bit 11
     */
    uint32_t ratio_reg;
    ratio_reg = read_register(offset | 0x50200, DRAM_REG_RATIO, d);
    bool ratio_io_ok = dram_has(d, DRAM_REG_RATIO);
    float ratio = (float)bit_slice(ratio_reg, 7, 0) / 3.0f;
    float mem_freq = ratio * 200.0f;
    d->data_rate_mts = mem_freq;
    d->data_rate_info = (value_info_t){
        dram_has(d, DRAM_REG_RATIO) && mem_freq > 0.0f, VALUE_SOURCE_SMN, VALUE_UNIT_MTS
    };
    d->ddr4_ratio_available = ratio_io_ok && ratio > 0.0f;

    uint32_t cmd2t_bit = bit_slice(ratio_reg, 10, 10);
    if (dram_has(d, DRAM_REG_RATIO))
        snprintf(d->cmd2t, sizeof(d->cmd2t), "%s", cmd2t_bit ? "2T" : "1T");
    d->gdm_enabled = bit_slice(ratio_reg, 11, 11) == 1;
    uint32_t refresh_reg;
    refresh_reg = read_register(offset | 0x5012C, DRAM_REG_REFRESH, d);
    d->ddr4_fgr_io_ok = dram_has(d, DRAM_REG_REFRESH);
    d->power_down_enabled = bit_slice(refresh_reg, 28, 28) == 1;

    read_common_timings(offset, d);

    /* RFC - DDR4: first non-default from 2 registers */
    uint32_t trfc0, trfc1;
    bool rfc0_io_ok = try_read_smn(offset | 0x50260, &trfc0);
    bool rfc1_io_ok = try_read_smn(offset | 0x50264, &trfc1);
    uint32_t trfc_reg = (trfc0 != trfc1) ? ((trfc0 != 0x21060138) ? trfc0 : trfc1) : trfc0;
    d->ddr4_rfc_available = rfc0_io_ok && rfc1_io_ok && trfc_reg != 0;
    if (d->ddr4_rfc_available) d->available |= DRAM_VALUE_RFC;
    if (trfc_reg) {
        d->rfc  = bit_slice(trfc_reg, 10, 0);
        d->rfc2 = bit_slice(trfc_reg, 21, 11);
        d->rfc4 = bit_slice(trfc_reg, 31, 22);
    }
    d->fgr = bit_slice(refresh_reg, 18, 16);

    convert_timings(d);
}

dram_layout_t dram_layout_for_codename(int codename_index)
{
    switch (codename_index) {
    case 20: case 21: /* Raphael (Ryzen 7000), Phoenix (Ryzen 7040/8040) — Zen4 DDR5 */
    case 22: case 24: /* Strix Point / Hawk Point — Zen5/Zen4c DDR5 */
    case 23:          /* Granite Ridge (Ryzen 9000) — Zen5 DDR5 */
        return DRAM_LAYOUT_DDR5;
    case 2:  /* Renoir (APU, DDR4/LPDDR4) */
    case 3:  /* Picasso (APU, DDR4) */
    case 7:  /* Raven Ridge (APU, DDR4) */
    case 8:  /* Raven Ridge 2 (APU, DDR4) */
    case 14: /* Cezanne (APU, DDR4) */
    case 16: /* Dali (APU, DDR4) */
    case 17: /* Lucienne (APU, DDR4/LPDDR4) */
    case 4: case 9: case 10: case 12: case 18: case 19:
        /* Matisse, Summit, Pinnacle, Vermeer, Naples, Chagall (DDR4) */
        return DRAM_LAYOUT_DDR4;
    default:
        return DRAM_LAYOUT_NONE;
    }
}

void dram_read_timings(int codename_index, dram_timings_t *out)
{
    memset(out, 0, sizeof(*out));
    out->layout = dram_layout_for_codename(codename_index);
    if (out->layout == DRAM_LAYOUT_DDR4) read_ddr4_timings(out);
    else if (out->layout == DRAM_LAYOUT_DDR5) read_ddr5_timings(out);
}
