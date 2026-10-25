/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AOD_AML_H
#define AOD_AML_H
/* Pure byte decoder; no AML evaluation or hardware access. */
static inline int aod_aml_integer(const unsigned char *p, unsigned long n,
                                  unsigned long long *value)
{
    unsigned int width, i;
    unsigned long long v = 0;
    if (!n)
        return 0;
    switch (p[0]) {
    case 0x00: *value = 0; return 1;
    case 0x01: *value = 1; return 1;
    case 0xff: *value = ~0ULL; return 1;
    case 0x0a: width = 1; break;
    case 0x0b: width = 2; break;
    case 0x0c: width = 4; break;
    case 0x0e: width = 8; break;
    default: return 0; /* Names and expressions require an AML interpreter. */
    }
    if (n < width + 1)
        return 0;
    for (i = 0; i < width; i++)
        v |= (unsigned long long)p[i + 1] << (8 * i);
    *value = v;
    return width + 1;
}

/* Administrative bound, not a firmware layout size. Fail closed above 1 MiB. */
#define AOD_MAX_REGION_SIZE (1024U * 1024U)
static inline int aod_aml_region(const unsigned char *p, unsigned long n,
                                 unsigned long long *address,
                                 unsigned long long *length)
{
    unsigned long long addr, len;
    int used = aod_aml_integer(p, n, &addr);
    if (!used || !aod_aml_integer(p + used, n - used, &len))
        return 0;
    if (!addr || len < 4 || len > AOD_MAX_REGION_SIZE ||
        addr > ~0ULL - (len - 1))
        return 0;
    *address = addr;
    *length = len;
    return 1;
}

static inline int aod_u32_fits(int offset, unsigned long size)
{
    return offset > 0 && size >= 4 && (unsigned long)offset <= size - 4;
}
#endif
