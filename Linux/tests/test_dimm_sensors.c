#define _XOPEN_SOURCE 700
#ifdef NDEBUG
#error "Assertions are required"
#endif
#include "dimm_sensors.h"
#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail_readdir, fail_closedir;
struct dirent *__real_readdir(DIR *dir);
int __real_closedir(DIR *dir);
struct dirent *__wrap_readdir(DIR *dir)
{
    if (fail_readdir) { fail_readdir = 0; errno = EIO; return NULL; }
    return __real_readdir(dir);
}
int __wrap_closedir(DIR *dir)
{
    int result = __real_closedir(dir);
    if (fail_closedir) { fail_closedir = 0; errno = EIO; return -1; }
    return result;
}

static char root[512];
static void pathf(char *out, size_t len, const char *fmt, ...)
{
    va_list args; va_start(args,fmt);
    int n=vsnprintf(out,len,fmt,args); va_end(args);
    assert(n>=0 && (size_t)n<len);
}
static void mkdirs(const char *name)
{
    char p[1024]; pathf(p,sizeof(p),"%s",name);
    for (char *c=p+1; *c; ++c) if (*c=='/') {
        *c=0; assert(mkdir(p,0700)==0 || errno==EEXIST); *c='/';
    }
    assert(mkdir(p,0700)==0 || errno==EEXIST);
}
static void write_bytes(const char *path, const void *data, size_t len)
{
    FILE *f=fopen(path,"wb"); assert(f);
    assert(fwrite(data,1,len,f)==len); assert(fclose(f)==0);
}
static void write_text(const char *path, const char *text)
{ write_bytes(path,text,strlen(text)); }
static void driver(const char *device, const char *name)
{
    char target[1024],link[1024];
    pathf(target,sizeof(target),"%s/drivers/%s",root,name); mkdirs(target);
    pathf(link,sizeof(link),"%s/driver",device); assert(symlink(target,link)==0);
}
static void make_spd(unsigned bus, unsigned addr, mem_type_t type,
                     const unsigned char serial[4], const char *part)
{
    char device[1024],path[1024];
    pathf(device,sizeof(device),"%s/devices/i2c-%u/%u-%04x",root,bus,bus,addr);
    mkdirs(device); driver(device,type==MEM_DDR5 ? "spd5118":"ee1004");
    pathf(path,sizeof(path),"%s/bus/i2c/devices/%u-%04x",root,bus,addr);
    assert(symlink(device,path)==0);
    unsigned char bytes[1024]={0};
    size_t len=type==MEM_DDR5 ? 1024:512;
    bytes[2]=type==MEM_DDR5 ? 0x12:0x0c;
    size_t sn=type==MEM_DDR5 ? 517:325, pn=type==MEM_DDR5 ? 521:329;
    memcpy(bytes+sn,serial,4); memset(bytes+pn,' ',type==MEM_DDR5 ? 30:20);
    memcpy(bytes+pn,part,strlen(part));
    pathf(path,sizeof(path),"%s/eeprom",device); write_bytes(path,bytes,len);
}
static void make_sensor(unsigned bus,unsigned addr,const char *name,int index,const char *temp)
{
    char device[1024],hwmon[1024],path[1024];
    pathf(device,sizeof(device),"%s/devices/i2c-%u/%u-%04x",root,bus,bus,addr);
    mkdirs(device);
    pathf(hwmon,sizeof(hwmon),"%s/class/hwmon/hwmon%d",root,index); mkdirs(hwmon);
    pathf(path,sizeof(path),"%s/device",hwmon); assert(symlink(device,path)==0);
    pathf(path,sizeof(path),"%s/name",hwmon); write_text(path,name);
    pathf(path,sizeof(path),"%s/temp1_input",hwmon); write_text(path,temp);
}
static void module(memory_module_t *m,const char *serial,mem_type_t type,const char *part)
{
    memset(m,0,sizeof(*m)); m->type=type;
    pathf(m->serial_number,sizeof(m->serial_number),"%s",serial);
    pathf(m->part_number,sizeof(m->part_number),"%s",part);
}
static int rm_entry(const char *p,const struct stat *s,int type,struct FTW *f)
{ (void)s;(void)type;(void)f;return remove(p); }
static void start(void)
{
    pathf(root,sizeof(root),"/tmp/tuxtimings-dimm.XXXXXX"); assert(mkdtemp(root));
    char p[1024]; pathf(p,sizeof(p),"%s/bus/i2c/devices",root);mkdirs(p);
    pathf(p,sizeof(p),"%s/class/hwmon",root);mkdirs(p);
}
static void finish(dimm_sensors_cache_t *cache)
{ dimm_sensors_destroy(cache); assert(nftw(root,rm_entry,16,FTW_DEPTH|FTW_PHYS)==0); }
static void refresh(dimm_sensors_cache_t *cache,memory_module_t *m,int n,smu_metrics_t *metrics)
{ dimm_sensors_update(cache,root,m,n,true,metrics); }
static const unsigned char sn_a[]={0x12,0x34,0x56,0x78},sn_b[]={0xab,0xcd,0xef,0x12};

static void test_identity(void)
{
    unsigned char bytes[1024]={0}; spd_identity_t id;
    bytes[2]=0x0c; memcpy(bytes+325,sn_a,4); memset(bytes+329,' ',20);
    memcpy(bytes+329,"  SAME-PART",11);
    assert(dimm_spd_identity(bytes,512,&id));
    assert(id.type==MEM_DDR4 && id.serial_valid && id.part_valid);
    assert(!strcmp(id.serial,"12345678") && !strcmp(id.part,"SAME-PART"));
    assert(!dimm_spd_identity(bytes,348,&id));
    memset(bytes+325,0,4); assert(dimm_spd_identity(bytes,512,&id) && !id.serial_valid);
    memset(bytes+325,0xff,4); assert(dimm_spd_identity(bytes,512,&id) && !id.serial_valid);
    bytes[2]=0; assert(!dimm_spd_identity(bytes,512,&id));
    memset(bytes,0,sizeof(bytes));bytes[2]=0x12;memcpy(bytes+517,sn_b,4);
    memset(bytes+521,' ',30);memcpy(bytes+521,"DDR5-PART",9);
    assert(dimm_spd_identity(bytes,1024,&id) && id.type==MEM_DDR5);
    assert(!strcmp(id.serial,"ABCDEF12"));
    assert(!dimm_spd_identity(bytes,550,&id));
}
static void test_mapping_and_measurement(void)
{
    start();make_spd(1,0x52,MEM_DDR4,sn_a,"SAME-PART");make_spd(1,0x53,MEM_DDR4,sn_b,"SAME-PART");
    make_sensor(1,0x1a,"jc42\n",30,"0\n");make_sensor(1,0x1b,"jc42\n",2,"28500\n");
    dimm_sensors_cache_t *cache=dimm_sensors_create();assert(cache);
    memory_module_t m[2];smu_metrics_t metrics={0};
    module(m,"0xabcdef12",MEM_DDR4,"SAME-PART ");module(m+1," 12345678 ",MEM_DDR4,"SAME-PART");
    refresh(cache,m,2,&metrics);
    assert(m[0].temp_state==DIMM_TEMP_ASSOCIATED && m[0].temperature_available && m[0].temperature_c==28.5f);
    assert(m[1].temp_state==DIMM_TEMP_ASSOCIATED && m[1].temperature_available && m[1].temperature_c==0);
    assert(strstr(m[0].temperature_sensor,"1-001b"));
    assert(metrics.spd_temps_count==2 && metrics.spd_temps_available[0] && metrics.spd_temps_available[1]);
    memory_module_t swapped=m[0];m[0]=m[1];m[1]=swapped;refresh(cache,m,2,&metrics);
    assert(m[0].temperature_c==0 && m[1].temperature_c==28.5f);
    char p[1024];pathf(p,sizeof(p),"%s/class/hwmon/hwmon2/temp1_input",root);write_text(p,"error\n");
    refresh(cache,m,2,&metrics);assert(m[1].temp_state==DIMM_TEMP_ASSOCIATED && !m[1].temperature_available);
    assert(m[1].temperature_c==0 && !metrics.spd_temps_available[1]);
    write_text(p,"28000 extra\n");refresh(cache,m,2,&metrics);assert(!m[1].temperature_available);
    write_text(p,"29000\n");refresh(cache,m,2,&metrics);assert(m[1].temperature_available && m[1].temperature_c==29);
    pathf(p,sizeof(p),"%s/class/hwmon/hwmon2",root);assert(nftw(p,rm_entry,16,FTW_DEPTH|FTW_PHYS)==0);
    refresh(cache,m,2,&metrics);assert(m[1].temp_state==DIMM_TEMP_SENSOR_MISSING && !m[1].temperature_available);
    make_sensor(1,0x1b,"jc42\n",87,"30000\n");refresh(cache,m,2,&metrics);assert(m[1].temperature_c==30);
    module(m,"00000000",MEM_DDR4,"SAME-PART");refresh(cache,m,2,&metrics);assert(m[0].temp_state==DIMM_TEMP_IDENTITY_MISSING);
    module(m,"FFFFFFFF",MEM_DDR4,"SAME-PART");refresh(cache,m,2,&metrics);assert(!m[0].temperature_available);
    module(m,"ABCDEF12",MEM_DDR4,"SAME-PART");refresh(cache,m,2,&metrics);
    assert(m[0].temp_state==DIMM_TEMP_AMBIGUOUS && m[1].temp_state==DIMM_TEMP_AMBIGUOUS);
    module(m,"12345678",MEM_DDR4,"WRONG");refresh(cache,m,2,&metrics);assert(!m[0].temperature_available);
    module(m,"12345678",MEM_UNKNOWN,"SAME-PART");refresh(cache,m,2,&metrics);assert(m[0].temp_state==DIMM_TEMP_INCOMPATIBLE);
    module(m,"12345678",MEM_LPDDR4,"SAME-PART");refresh(cache,m,2,&metrics);assert(!m[0].temperature_available);
    dimm_sensors_update(cache,root,m,2,false,&metrics);assert(m[0].temp_state==DIMM_TEMP_INVENTORY_INCOMPLETE);
    finish(cache);
}
static void test_bus_identity_duplicates_and_cache(void)
{
    start();make_spd(1,0x52,MEM_DDR4,sn_a,"SAME-PART");make_sensor(1,0x1a,"jc42\n",1,"28000\n");
    make_spd(2,0x52,MEM_DDR4,sn_b,"SAME-PART");make_sensor(2,0x1a,"jc42\n",2,"32000\n");
    dimm_sensors_cache_t *cache=dimm_sensors_create();memory_module_t m;smu_metrics_t metrics={0};
    module(&m,"ABCDEF12",MEM_DDR4,"SAME-PART");refresh(cache,&m,1,&metrics);
    assert(m.temperature_available && m.temperature_c==32 && strstr(m.temperature_sensor,"2-001a"));
    char p[1024];pathf(p,sizeof(p),"%s/devices/i2c-2/2-0052/eeprom",root);
    FILE *f=fopen(p,"r+b");assert(f);assert(fseek(f,325,SEEK_SET)==0);assert(fwrite(sn_a,1,4,f)==4);assert(!fclose(f));
    refresh(cache,&m,1,&metrics);assert(m.temperature_available && m.temperature_c==32); /* identity cached */
    make_sensor(2,0x1b,"jc42\n",3,"33000\n");refresh(cache,&m,1,&metrics);
    assert(!m.temperature_available); /* topology changed: identities reread */
    module(&m,"12345678",MEM_DDR4,"SAME-PART");refresh(cache,&m,1,&metrics);assert(m.temp_state==DIMM_TEMP_AMBIGUOUS);
    finish(cache);
}
static void test_ddr5_and_bad_eeprom(void)
{
    start();make_spd(3,0x50,MEM_DDR5,sn_a,"DDR5-PART");make_sensor(3,0x50,"spd5118\n",4,"31000\n");
    dimm_sensors_cache_t *cache=dimm_sensors_create();memory_module_t m;smu_metrics_t metrics={0};
    module(&m,"12345678",MEM_DDR5,"DDR5-PART");refresh(cache,&m,1,&metrics);
    assert(m.temp_state==DIMM_TEMP_ASSOCIATED && m.temperature_available && m.temperature_c==31);
    char p[1024];pathf(p,sizeof(p),"%s/devices/i2c-3/3-0050/eeprom",root);assert(!unlink(p));
    refresh(cache,&m,1,&metrics);assert(m.temp_state==DIMM_TEMP_SPD_UNAVAILABLE && !m.temperature_available);
    assert(metrics.spd_temps_count==1 && metrics.spd_temps_available[0]);
    unsigned char short_data[550]={0};short_data[2]=0x12;write_bytes(p,short_data,sizeof(short_data));
    refresh(cache,&m,1,&metrics);assert(!m.temperature_available);
    finish(cache);
}
static void test_overflow(void)
{
    start();dimm_sensors_cache_t *cache=dimm_sensors_create();memory_module_t m;smu_metrics_t metrics={0};
    module(&m,"12345678",MEM_DDR4,"SAME-PART");
    for(unsigned i=0;i<40;i++) make_spd(i+1,0x50,MEM_DDR4,sn_a,"SAME-PART");
    make_sensor(1,0x18,"jc42\n",1,"27000\n");refresh(cache,&m,1,&metrics);
    assert(m.temp_state==DIMM_TEMP_INVENTORY_INCOMPLETE && !m.temperature_available);
    finish(cache);
}
static void test_ambiguous_incomplete_and_strict_inputs(void)
{
    start();make_spd(1,0x52,MEM_DDR4,sn_a,"SAME-PART");make_sensor(1,0x1a,"jc42\n",1,"28000\n");
    dimm_sensors_cache_t *cache=dimm_sensors_create();memory_module_t m;smu_metrics_t metrics={0};
    module(&m,"12345678",MEM_DDR4,"SAME-PART");refresh(cache,&m,1,&metrics);assert(m.temperature_available);
    const char *bad_serials[]={"12 34 56 78","0x1234","unknown","123456789","FFFFFFFF","00000000"};
    for(unsigned i=0;i<sizeof(bad_serials)/sizeof(*bad_serials);i++) {
        module(&m,bad_serials[i],MEM_DDR4,"SAME-PART");refresh(cache,&m,1,&metrics);
        assert(m.temp_state==DIMM_TEMP_IDENTITY_MISSING && !m.temperature_available);
    }
    module(&m,"12345678",MEM_DDR4,"SAME-PART");
    char path[1024];pathf(path,sizeof(path),"%s/class/hwmon/hwmon1/temp1_input",root);
    const char *bad_temps[]={"","150001\n","-1\n","NaN\n","999999999999999999999999\n"};
    for(unsigned i=0;i<sizeof(bad_temps)/sizeof(*bad_temps);i++) {
        write_text(path,bad_temps[i]);refresh(cache,&m,1,&metrics);
        assert(m.temp_state==DIMM_TEMP_ASSOCIATED && !m.temperature_available && m.temperature_c==0);
    }
    const char embedded[]={ '0', 0, '1' };write_bytes(path,embedded,sizeof(embedded));
    refresh(cache,&m,1,&metrics);assert(!m.temperature_available);
    write_text(path,"27000\n");
    make_spd(2,0x52,MEM_DDR4,sn_a," "); /* duplicate still counts without part or sensor */
    refresh(cache,&m,1,&metrics);assert(m.temp_state==DIMM_TEMP_AMBIGUOUS);
    pathf(path,sizeof(path),"%s/devices/i2c-2/2-0052/eeprom",root);assert(!unlink(path));
    refresh(cache,&m,1,&metrics);assert(m.temp_state==DIMM_TEMP_SPD_UNAVAILABLE);
    unsigned char short_data[511]={0};short_data[2]=0x0c;write_bytes(path,short_data,sizeof(short_data));
    refresh(cache,&m,1,&metrics);assert(m.temp_state==DIMM_TEMP_SPD_UNAVAILABLE);
    assert(!unlink(path));assert(mkdir(path,0700)==0); /* open/read failure, no guessed fallback */
    refresh(cache,&m,1,&metrics);assert(!m.temperature_available);
    finish(cache);

    start();make_spd(3,0x50,MEM_DDR5,sn_a,"DDR5-PART");
    make_sensor(3,0x51,"spd5118\n",7,"31000\n"); /* same bus is insufficient in DDR5 */
    cache=dimm_sensors_create();module(&m,"12345678",MEM_DDR5,"DDR5-PART");
    refresh(cache,&m,1,&metrics);assert(m.temp_state==DIMM_TEMP_SENSOR_MISSING);
    make_sensor(3,0x50,"spd5118\n",8,"32000\n");
    refresh(cache,&m,1,&metrics);assert(m.temperature_available && m.temperature_c==32);
    finish(cache);
}
static void test_spd_driver_discovery_failures(void)
{
    start();make_spd(1,0x52,MEM_DDR4,sn_a,"SAME-PART");make_sensor(1,0x1a,"jc42\n",1,"28000\n");
    make_spd(2,0x53,MEM_DDR4,sn_b,"SAME-PART");
    dimm_sensors_cache_t *cache=dimm_sensors_create();memory_module_t m;smu_metrics_t metrics={0};
    module(&m,"12345678",MEM_DDR4,"SAME-PART");refresh(cache,&m,1,&metrics);assert(m.temperature_available);
    char device[1024],link[1024],target[1024];
    pathf(device,sizeof(device),"%s/devices/i2c-2/2-0053",root);
    pathf(link,sizeof(link),"%s/driver",device);assert(!unlink(link));
    pathf(target,sizeof(target),"%s/drivers/missing",root);assert(symlink(target,link)==0);
    refresh(cache,&m,1,&metrics);
    assert(m.temp_state==DIMM_TEMP_INVENTORY_INCOMPLETE && !m.temperature_available);
    assert(m.temperature_c==0 && !m.temperature_sensor[0]);
    assert(metrics.spd_temps_count==1 && metrics.spd_temps_available[0] && metrics.spd_temps_c[0]==28);
    assert(!unlink(link)); /* a missing driver is also an unresolved SPD candidate */
    refresh(cache,&m,1,&metrics);
    assert(m.temp_state==DIMM_TEMP_INVENTORY_INCOMPLETE && !m.temperature_available);
    driver(device,"ee1004");refresh(cache,&m,1,&metrics);
    assert(m.temp_state==DIMM_TEMP_ASSOCIATED && m.temperature_available && m.temperature_c==28);
    assert(!unlink(link));driver(device,"at24"); /* a resolved non-SPD driver is deliberately ignored */
    refresh(cache,&m,1,&metrics);assert(m.temp_state==DIMM_TEMP_ASSOCIATED && m.temperature_available);
    pathf(device,sizeof(device),"%s/devices/i2c-3/3-0040",root);mkdirs(device);
    pathf(link,sizeof(link),"%s/bus/i2c/devices/3-0040",root);assert(symlink(device,link)==0);
    refresh(cache,&m,1,&metrics);assert(m.temp_state==DIMM_TEMP_ASSOCIATED && m.temperature_available);
    pathf(link,sizeof(link),"%s/driver",device);assert(symlink(target,link)==0);
    refresh(cache,&m,1,&metrics);assert(m.temp_state==DIMM_TEMP_ASSOCIATED && m.temperature_available);
    finish(cache);
}
static void test_duplicate_matching_sensors(void)
{
    start();make_spd(1,0x52,MEM_DDR4,sn_a,"SAME-PART");make_sensor(1,0x1a,"jc42\n",1,"28000\n");
    dimm_sensors_cache_t *cache=dimm_sensors_create();memory_module_t m;smu_metrics_t metrics={0};
    module(&m,"12345678",MEM_DDR4,"SAME-PART");refresh(cache,&m,1,&metrics);assert(m.temperature_available);
    make_sensor(1,0x1a,"jc42\n",2,"31000\n"); /* both hwmons resolve to the same device */
    refresh(cache,&m,1,&metrics);
    assert(m.temp_state==DIMM_TEMP_SENSOR_MISSING && !m.temperature_available);
    assert(m.temperature_c==0 && !m.temperature_sensor[0]);
    assert(metrics.spd_temps_count==2 && metrics.spd_temps_available[0] && metrics.spd_temps_available[1]);
    assert(metrics.spd_temps_c[0]==28 && metrics.spd_temps_c[1]==31);
    finish(cache);
}
static void test_enumeration_failures(void)
{
    start();make_spd(1,0x52,MEM_DDR4,sn_a,"SAME-PART");make_sensor(1,0x1a,"jc42\n",1,"28000\n");
    dimm_sensors_cache_t *cache=dimm_sensors_create();memory_module_t m;smu_metrics_t metrics={0};
    module(&m,"12345678",MEM_DDR4,"SAME-PART");refresh(cache,&m,1,&metrics);assert(m.temperature_available);
    fail_readdir=1;refresh(cache,&m,1,&metrics);
    assert(m.temp_state==DIMM_TEMP_INVENTORY_INCOMPLETE && !m.temperature_available);
    refresh(cache,&m,1,&metrics);assert(m.temperature_available);
    fail_closedir=1;refresh(cache,&m,1,&metrics);
    assert(m.temp_state==DIMM_TEMP_INVENTORY_INCOMPLETE && !m.temperature_available);
    refresh(cache,&m,1,&metrics);assert(m.temperature_available);
    finish(cache);
}
int main(void)
{
    test_identity();test_mapping_and_measurement();test_bus_identity_duplicates_and_cache();
    test_ddr5_and_bad_eeprom();test_overflow();test_enumeration_failures();
    test_spd_driver_discovery_failures();test_duplicate_matching_sensors();
    test_ambiguous_incomplete_and_strict_inputs();
    puts("DIMM identity, topology, cache and temperature tests passed; synthetic sysfs only");
    return 0;
}
