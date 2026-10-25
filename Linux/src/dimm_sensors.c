#define _XOPEN_SOURCE 700
#include "dimm_sensors.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define INVENTORY_LIMIT 32
#define SENSOR_PATH 1024

typedef struct {
    bool present;
    dev_t device;
    ino_t inode;
    mode_t mode;
    off_t size;
} file_stamp_t;

typedef struct {
    char device[SENSOR_PATH], adapter[SENSOR_PATH];
    unsigned address;
    mem_type_t driver_type;
    file_stamp_t device_stamp, eeprom_stamp;
    bool read_ok;
    spd_identity_t identity;
} spd_record_t;

typedef struct {
    char device[SENSOR_PATH], adapter[SENSOR_PATH], temp_path[SENSOR_PATH];
    char driver[16], label[STR_SHORT];
    unsigned address;
    bool i2c, available;
    float temperature;
    file_stamp_t device_stamp, temp_stamp;
} sensor_record_t;

typedef struct {
    spd_record_t spds[INVENTORY_LIMIT];
    sensor_record_t sensors[INVENTORY_LIMIT];
    int spd_count, sensor_count;
    bool complete;
} inventory_t;

struct dimm_sensors_cache {
    inventory_t previous, current;
    bool initialized;
};

static bool path_join(char *out, size_t capacity, const char *left, const char *right)
{
    int n = snprintf(out, capacity, "%s/%s", left, right);
    return n >= 0 && (size_t)n < capacity;
}

static bool copy_path(char *out, const char *source)
{
    size_t len = strlen(source);
    if (len >= SENSOR_PATH) return false;
    memcpy(out, source, len + 1);
    return true;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* Trim only peripheral padding. No byte reversal, fuzzy matching or numeric
 * interpretation of serials: SMBIOS must carry the same four hex bytes. */
static bool normalize_serial(const char *source, char out[9])
{
    out[0] = '\0';
    while (isspace((unsigned char)*source)) source++;
    size_t len = strlen(source);
    while (len && isspace((unsigned char)source[len - 1])) len--;
    if (len == 10 && source[0] == '0' && (source[1] == 'x' || source[1] == 'X')) {
        source += 2;
        len -= 2;
    }
    if (len != 8) return false;
    for (size_t i = 0; i < 8; i++) {
        unsigned char c = (unsigned char)source[i];
        if (!isxdigit(c)) { out[0] = '\0'; return false; }
        out[i] = (char)toupper(c);
    }
    out[8] = '\0';
    return strcmp(out, "00000000") && strcmp(out, "FFFFFFFF");
}

static bool normalize_part(const char *source, size_t len, char *out, size_t capacity)
{
    out[0] = '\0';
    size_t end = 0;
    while (end < len && source[end]) end++;
    for (size_t i = end; i < len; i++)
        if (source[i] && source[i] != ' ') return false;
    while (end && isspace((unsigned char)source[end - 1])) end--;
    size_t start = 0;
    while (start < end && isspace((unsigned char)source[start])) start++;
    if (start == end || end - start >= capacity) return false;
    for (size_t i = start; i < end; i++)
        if ((unsigned char)source[i] < 0x20 || (unsigned char)source[i] > 0x7e) return false;
    memcpy(out, source + start, end - start);
    out[end - start] = '\0';
    return strcmp(out, "Unknown") && strcmp(out, "Not Specified") && strcmp(out, "NO DIMM");
}

bool dimm_spd_identity(const uint8_t *bytes, size_t length, spd_identity_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!bytes || length < 3) return false;
    size_t serial, part, part_length, expected;
    if (bytes[2] == 0x0c) {
        out->type = MEM_DDR4;
        serial = 325; part = 329; part_length = 20; expected = 512;
    } else if (bytes[2] == 0x12) {
        out->type = MEM_DDR5;
        serial = 517; part = 521; part_length = 30; expected = 1024;
    } else return false;
    if (length < expected) return false;
    char raw[9];
    snprintf(raw, sizeof(raw), "%02X%02X%02X%02X",
             bytes[serial], bytes[serial + 1], bytes[serial + 2], bytes[serial + 3]);
    out->serial_valid = normalize_serial(raw, out->serial);
    bool ascii_part = true;
    for (size_t i = 0; i < part_length; i++)
        if (bytes[part + i] && (bytes[part + i] < 0x20 || bytes[part + i] > 0x7e)) ascii_part = false;
    out->part_valid = ascii_part && normalize_part((const char *)bytes + part, part_length,
                                                   out->part, sizeof(out->part));
    return true;
}

static file_stamp_t stamp(const char *path)
{
    struct stat value;
    file_stamp_t result = {0};
    if (stat(path, &value) == 0) {
        result.present = true;
        result.device = value.st_dev;
        result.inode = value.st_ino;
        result.mode = value.st_mode;
        result.size = value.st_size;
    }
    return result;
}

static bool same_stamp(file_stamp_t a, file_stamp_t b)
{
    return a.present == b.present && (!a.present ||
           (a.device == b.device && a.inode == b.inode && a.mode == b.mode && a.size == b.size));
}

static bool read_text(const char *path, char *out, size_t capacity)
{
    out[0] = '\0';
    FILE *file = fopen(path, "r");
    if (!file) return false;
    size_t len = fread(out, 1, capacity - 1, file);
    bool ok = !ferror(file) && fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0) ok = false;
    if (!ok || memchr(out, '\0', len)) { out[0] = '\0'; return false; }
    out[len] = '\0';
    while (len && isspace((unsigned char)out[len - 1])) out[--len] = '\0';
    return true;
}

static bool i2c_address(const char *device, unsigned *address)
{
    unsigned bus, addr;
    int consumed = 0;
    const char *name = base_name(device);
    if (sscanf(name, "%u-%x%n", &bus, &addr, &consumed) != 2 || name[consumed] || addr > 0x7f)
        return false;
    (void)bus;
    *address = addr;
    return true;
}

static bool i2c_location(const char *device, char *adapter, unsigned *address)
{
    if (!i2c_address(device, address) || !copy_path(adapter, device)) return false;
    char *slash = strrchr(adapter, '/');
    if (!slash) return false;
    *slash = '\0';
    return true;
}

static int compare_spd(const void *a, const void *b)
{ return strcmp(((const spd_record_t *)a)->device, ((const spd_record_t *)b)->device); }
static int compare_sensor(const void *a, const void *b)
{
    const sensor_record_t *left = a, *right = b;
    int order = strcmp(left->driver, right->driver);
    if (!order) order = strcmp(left->device, right->device);
    return order ? order : strcmp(left->temp_path, right->temp_path);
}

static void discover_spds(const char *root, inventory_t *inventory)
{
    char directory[SENSOR_PATH];
    if (!path_join(directory, sizeof(directory), root, "bus/i2c/devices")) {
        inventory->complete = false; return;
    }
    DIR *dir = opendir(directory);
    if (!dir) { inventory->complete = false; return; }
    struct dirent *entry;
    for (;;) {
        errno = 0;
        entry = readdir(dir);
        if (!entry) {
            if (errno) inventory->complete = false;
            break;
        }
        if (entry->d_name[0] == '.') continue;
        char path[SENSOR_PATH], link[SENSOR_PATH], resolved[PATH_MAX];
        unsigned candidate_address;
        bool spd_candidate = i2c_address(entry->d_name, &candidate_address) &&
                             candidate_address >= 0x50 && candidate_address <= 0x57;
        if (!path_join(path, sizeof(path), directory, entry->d_name) ||
            !path_join(link, sizeof(link), path, "driver") || !realpath(link, resolved)) {
            /* An unresolved SPD-address entry could hide a duplicate serial. */
            if (spd_candidate) inventory->complete = false;
            continue;
        }
        mem_type_t type;
        if (!strcmp(base_name(resolved), "ee1004")) type = MEM_DDR4;
        else if (!strcmp(base_name(resolved), "spd5118")) type = MEM_DDR5;
        else continue;
        if (inventory->spd_count == INVENTORY_LIMIT) { inventory->complete = false; continue; }
        spd_record_t *record = &inventory->spds[inventory->spd_count];
        if (!realpath(path, resolved) || !copy_path(record->device, resolved) ||
            !i2c_location(record->device, record->adapter, &record->address) ||
            record->address < 0x50 || record->address > 0x57) {
            inventory->complete = false; continue;
        }
        record->driver_type = type;
        record->device_stamp = stamp(path);
        if (!path_join(link, sizeof(link), path, "eeprom")) { inventory->complete = false; continue; }
        record->eeprom_stamp = stamp(link);
        inventory->spd_count++;
    }
    if (closedir(dir) != 0) inventory->complete = false;
    qsort(inventory->spds, (size_t)inventory->spd_count, sizeof(inventory->spds[0]), compare_spd);
}

static void discover_sensors(const char *root, inventory_t *inventory)
{
    char directory[SENSOR_PATH];
    if (!path_join(directory, sizeof(directory), root, "class/hwmon")) {
        inventory->complete = false; return;
    }
    DIR *dir = opendir(directory);
    if (!dir) { inventory->complete = false; return; }
    struct dirent *entry;
    for (;;) {
        errno = 0;
        entry = readdir(dir);
        if (!entry) {
            if (errno) inventory->complete = false;
            break;
        }
        if (entry->d_name[0] == '.') continue;
        char path[SENSOR_PATH], file[SENSOR_PATH], name[64], resolved[PATH_MAX];
        if (!path_join(path, sizeof(path), directory, entry->d_name) ||
            !path_join(file, sizeof(file), path, "name") || !read_text(file, name, sizeof(name))) continue;
        if (strcmp(name, "jc42") && strcmp(name, "spd5118")) continue;
        if (inventory->sensor_count == INVENTORY_LIMIT) { inventory->complete = false; continue; }
        sensor_record_t *sensor = &inventory->sensors[inventory->sensor_count];
        snprintf(sensor->driver, sizeof(sensor->driver), "%.15s", name);
        if (!path_join(file, sizeof(file), path, "device")) { inventory->complete = false; continue; }
        if (realpath(file, resolved)) {
            if (!copy_path(sensor->device, resolved)) { inventory->complete = false; continue; }
            sensor->device_stamp = stamp(file);
            sensor->i2c = i2c_location(sensor->device, sensor->adapter, &sensor->address);
        }
        if (!path_join(sensor->temp_path, sizeof(sensor->temp_path), path, "temp1_input")) {
            inventory->complete = false; continue;
        }
        sensor->temp_stamp = stamp(sensor->temp_path);
        const char *id = sensor->device[0] ? base_name(sensor->device) : entry->d_name;
        char label[STR_SHORT];
        snprintf(label, sizeof(label), "%.15s %.40s", name, id);
        memcpy(sensor->label, label, sizeof(label));
        inventory->sensor_count++;
    }
    if (closedir(dir) != 0) inventory->complete = false;
    qsort(inventory->sensors, (size_t)inventory->sensor_count, sizeof(inventory->sensors[0]), compare_sensor);
}

static bool same_topology(const inventory_t *a, const inventory_t *b)
{
    if (a->complete != b->complete || a->spd_count != b->spd_count || a->sensor_count != b->sensor_count)
        return false;
    for (int i = 0; i < a->spd_count; i++) {
        const spd_record_t *x = &a->spds[i], *y = &b->spds[i];
        if (strcmp(x->device, y->device) || x->driver_type != y->driver_type ||
            !same_stamp(x->device_stamp, y->device_stamp) || !same_stamp(x->eeprom_stamp, y->eeprom_stamp))
            return false;
    }
    for (int i = 0; i < a->sensor_count; i++) {
        const sensor_record_t *x = &a->sensors[i], *y = &b->sensors[i];
        if (strcmp(x->device, y->device) || strcmp(x->driver, y->driver) ||
            !same_stamp(x->device_stamp, y->device_stamp) || !same_stamp(x->temp_stamp, y->temp_stamp))
            return false;
    }
    return true;
}

static void read_spd(spd_record_t *record)
{
    char path[SENSOR_PATH];
    if (!path_join(path, sizeof(path), record->device, "eeprom")) return;
    FILE *file = fopen(path, "rb");
    if (!file) return;
    uint8_t bytes[1024];
    size_t expected = record->driver_type == MEM_DDR4 ? 512 : 1024;
    size_t got = fread(bytes, 1, expected, file);
    bool ok = got == expected && !ferror(file);
    if (fclose(file) != 0) ok = false;
    record->read_ok = ok && dimm_spd_identity(bytes, got, &record->identity);
}

static void read_temperature(sensor_record_t *sensor)
{
    char text[64];
    sensor->available = false;
    sensor->temperature = 0;
    if (!read_text(sensor->temp_path, text, sizeof(text))) return;
    char *end;
    errno = 0;
    long raw = strtol(text, &end, 10);
    if (errno || end == text || *end || raw < 0 || raw > 150000) return;
    sensor->temperature = (float)raw / 1000.0f;
    sensor->available = true;
}

static bool same_sensor(const spd_record_t *spd, const sensor_record_t *sensor)
{
    if (!sensor->i2c) return false;
    if (spd->driver_type == MEM_DDR4)
        return !strcmp(sensor->driver, "jc42") && !strcmp(spd->adapter, sensor->adapter) &&
               sensor->address == (0x18u | (spd->address & 7));
    return !strcmp(sensor->driver, "spd5118") && !strcmp(spd->device, sensor->device);
}

static void associate(inventory_t *inventory, memory_module_t *modules, int count)
{
    for (int i = 0; i < count; i++) {
        memory_module_t *module = &modules[i];
        char serial[9], part[STR_LEN];
        if (module->type != MEM_DDR4 && module->type != MEM_DDR5) {
            module->temp_state = DIMM_TEMP_INCOMPATIBLE; continue;
        }
        if (!normalize_serial(module->serial_number, serial) ||
            !normalize_part(module->part_number, strlen(module->part_number), part, sizeof(part))) {
            module->temp_state = DIMM_TEMP_IDENTITY_MISSING; continue;
        }
        int module_matches = 0;
        for (int j = 0; j < count; j++) {
            char other[9];
            if (normalize_serial(modules[j].serial_number, other) && !strcmp(serial, other)) module_matches++;
        }
        int spd_matches = 0, spd_index = -1;
        bool unreadable = false;
        for (int j = 0; j < inventory->spd_count; j++) {
            spd_record_t *spd = &inventory->spds[j];
            if (!spd->read_ok) unreadable = true;
            if (spd->identity.serial_valid && !strcmp(serial, spd->identity.serial)) {
                spd_matches++; spd_index = j;
            }
        }
        if (module_matches > 1 || spd_matches > 1) {
            module->temp_state = DIMM_TEMP_AMBIGUOUS; continue;
        }
        /* An unreadable SPD could hide a duplicate serial: do not claim
         * uniqueness from an incomplete identity inventory. */
        if (unreadable) { module->temp_state = DIMM_TEMP_SPD_UNAVAILABLE; continue; }
        if (spd_matches != 1) continue;
        spd_record_t *spd = &inventory->spds[spd_index];
        if (spd->identity.type != module->type || spd->identity.type != spd->driver_type) {
            module->temp_state = DIMM_TEMP_INCOMPATIBLE; continue;
        }
        if (!spd->identity.part_valid || strcmp(part, spd->identity.part)) continue;
        int matches = 0, sensor_index = -1;
        for (int j = 0; j < inventory->sensor_count; j++)
            if (same_sensor(spd, &inventory->sensors[j])) { matches++; sensor_index = j; }
        if (matches != 1) { module->temp_state = DIMM_TEMP_SENSOR_MISSING; continue; }
        sensor_record_t *sensor = &inventory->sensors[sensor_index];
        module->temp_state = DIMM_TEMP_ASSOCIATED;
        module->temperature_available = sensor->available;
        module->temperature_c = sensor->temperature;
        memcpy(module->temperature_sensor, sensor->label, sizeof(module->temperature_sensor));
    }
}

dimm_sensors_cache_t *dimm_sensors_create(void)
{ return calloc(1, sizeof(dimm_sensors_cache_t)); }
void dimm_sensors_destroy(dimm_sensors_cache_t *cache)
{ free(cache); }

void dimm_sensors_update(dimm_sensors_cache_t *cache, const char *sysfs_root,
                         memory_module_t *modules, int module_count,
                         bool modules_complete, smu_metrics_t *metrics)
{
    metrics->spd_temps_count = 0;
    memset(metrics->spd_temps_c, 0, sizeof(metrics->spd_temps_c));
    memset(metrics->spd_temps_available, 0, sizeof(metrics->spd_temps_available));
    memset(metrics->spd_temp_labels, 0, sizeof(metrics->spd_temp_labels));
    for (int i = 0; i < module_count; i++) {
        modules[i].temp_state = DIMM_TEMP_UNASSOCIATED;
        modules[i].temperature_available = false;
        modules[i].temperature_c = 0;
        modules[i].temperature_sensor[0] = '\0';
    }
    if (!cache) return;
    inventory_t *inventory = &cache->current;
    memset(inventory, 0, sizeof(*inventory));
    inventory->complete = true;
    discover_spds(sysfs_root, inventory);
    discover_sensors(sysfs_root, inventory);
    bool reuse = cache->initialized && same_topology(inventory, &cache->previous);
    for (int i = 0; i < inventory->spd_count; i++) {
        if (reuse) {
            inventory->spds[i].read_ok = cache->previous.spds[i].read_ok;
            inventory->spds[i].identity = cache->previous.spds[i].identity;
        } else read_spd(&inventory->spds[i]);
    }
    cache->previous = *inventory;
    cache->initialized = true;
    for (int i = 0; i < inventory->sensor_count; i++) {
        sensor_record_t *sensor = &inventory->sensors[i];
        read_temperature(sensor);
        if (i < MAX_MODULES) {
            metrics->spd_temps_count++;
            metrics->spd_temps_c[i] = sensor->temperature;
            metrics->spd_temps_available[i] = sensor->available;
            memcpy(metrics->spd_temp_labels[i], sensor->label, sizeof(sensor->label));
        }
    }
    if (!modules_complete || !inventory->complete) {
        for (int i = 0; i < module_count; i++) modules[i].temp_state = DIMM_TEMP_INVENTORY_INCOMPLETE;
        return;
    }
    associate(inventory, modules, module_count);
}

const char *dimm_temperature_reason(dimm_temp_state_t state)
{
    switch (state) {
    case DIMM_TEMP_ASSOCIATED: return "Unique SMBIOS/SPD serial and part number; sensor linked by device topology.";
    case DIMM_TEMP_IDENTITY_MISSING: return "SMBIOS serial or part number is missing or unusable.";
    case DIMM_TEMP_AMBIGUOUS: return "The serial number is not unique in SMBIOS or SPD.";
    case DIMM_TEMP_SPD_UNAVAILABLE: return "An SPD identity is unreadable; uniqueness cannot be established.";
    case DIMM_TEMP_INCOMPATIBLE: return "Memory type is unknown or incompatible with the SPD format.";
    case DIMM_TEMP_INVENTORY_INCOMPLETE: return "The device or SMBIOS inventory is incomplete.";
    case DIMM_TEMP_SENSOR_MISSING: return "The DIMM SPD is identified, but no unique temperature sensor is present.";
    default: return "No proven SMBIOS-to-SPD association.";
    }
}
