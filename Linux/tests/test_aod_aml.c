/* SPDX-License-Identifier: GPL-2.0 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/aod-voltages/aod_aml.h"

int main(int argc, char **argv)
{
    unsigned long long a = 0, n = 0, v;
    unsigned char legacy[] = {0x0c,0x18,0x80,0xc0,0xca,0x0b,0x0c,0x22};
    unsigned char modern[] = {0x0e,0,0,0,0,1,0,0,0,0x0c,0xbb,0x24,0,0};
    unsigned char bad[] = {0xff,0x0b,0,0x20};
    unsigned char codes[] = {0x0a,0x0b,0x0c,0x0e};
    unsigned widths[] = {1,2,4,8};
    for (unsigned i=0;i<4;i++) {
        unsigned char b[9] = {0};
        b[0]=codes[i]; b[1]=42;
        for (unsigned j=0;j<=widths[i];j++)
            assert(!aod_aml_integer(b,j,&v));
        assert(aod_aml_integer(b,widths[i]+1,&v)==(int)widths[i]+1 && v==42);
    }
    assert(aod_aml_region(legacy,sizeof legacy,&a,&n));
    assert(a==0xcac08018ULL && n==0x220c);
    for (unsigned i=0;i<sizeof legacy;i++)
        assert(!aod_aml_region(legacy,i,&a,&n));
    assert(aod_aml_region(modern,sizeof modern,&a,&n));
    assert(a==0x100000000ULL && n==0x24bb);
    assert(!aod_aml_region(bad,sizeof bad,&a,&n)); /* Address overflow. */
    legacy[5]=0; /* Zero length. */
    assert(!aod_aml_region(legacy,sizeof legacy,&a,&n));
    legacy[5]=0x60; /* Local0: unevaluated expression. */
    assert(!aod_aml_region(legacy,sizeof legacy,&a,&n));
    modern[10]=0; modern[11]=0; modern[12]=0x20; /* 2 MiB. */
    assert(!aod_aml_region(modern,sizeof modern,&a,&n));
    assert(!aod_u32_fits(-1,0x220c));
    assert(!aod_u32_fits(0,0x220c));
    assert(!aod_u32_fits(INT_MAX,0x220c));
    assert(!aod_u32_fits(1,3));
    assert(aod_u32_fits(0x2208,0x220c));
    assert(!aod_u32_fits(0x2209,0x220c));
    /* Optional offline SSDT fixture; never reads hardware. */
    if (argc==2) {
        unsigned char data[65536];
        FILE *f=fopen(argv[1],"rb");
        assert(f);
        size_t size=fread(data,1,sizeof data,f);
        assert(!ferror(f)); fclose(f);
        unsigned found=0;
        for(size_t i=0;i+7<=size;i++) {
            if(memcmp(data+i,"\x5b\x80" "AODT\0",7)) continue;
            assert(aod_aml_region(data+i+7,size-i-7,&a,&n));
            assert(n==0x220c); found++;
        }
        assert(found==1);
    }
    puts("AOD constant and bounds tests passed; no hardware access");
    return 0;
}
