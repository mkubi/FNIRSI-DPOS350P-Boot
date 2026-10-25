#include <stddef.h>
#include <stdint.h>
#include "fat.h"
#include "sdmmc0.h"

#define MBR_PARTITION_TABLE 0x1BEU
#define MBR_ENTRY_SIZE      16U
#define FAT16_EOC_MIN       0xFFF8U
#define FAT16_BAD_CLUSTER   0xFFF7U
#define FAT32_EOC_MIN       0x0FFFFFF8U
#define FAT32_BAD_CLUSTER   0x0FFFFFF7U

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static int name_equal_11(const uint8_t *entry, const char name83[11])
{
    unsigned i;

    for (i = 0U; i < 11U; i++) {
        if (entry[i] != (uint8_t)name83[i])
            return 0;
    }
    return 1;
}

static int known_fat_partition(uint8_t type)
{
    return type == 0x04U || type == 0x06U || type == 0x0EU ||
           type == 0x0BU || type == 0x0CU;
}

static uint32_t cluster_lba(const fat_volume_t *v, uint32_t cluster)
{
    return v->data_begin_lba
         + (cluster - 2U) * (uint32_t)v->sectors_per_cluster;
}

static int cluster_is_eoc(const fat_volume_t *v, uint32_t cluster)
{
    if (v->fat_type == FAT_TYPE_16)
        return cluster >= FAT16_EOC_MIN;
    return cluster >= FAT32_EOC_MIN;
}

static int cluster_is_bad(const fat_volume_t *v, uint32_t cluster)
{
    if (v->fat_type == FAT_TYPE_16)
        return cluster == FAT16_BAD_CLUSTER;
    return cluster == FAT32_BAD_CLUSTER;
}

static int next_cluster(const fat_volume_t *v, uint32_t cluster,
                        uint32_t *next)
{
    static uint32_t fat_words[FAT_SECTOR_SIZE / 4U];
    uint8_t *sector = (uint8_t *)fat_words;
    uint32_t entry_size;
    uint32_t fat_offset;
    uint32_t fat_sector;
    uint32_t entry_offset;
    int r;

    entry_size = v->fat_type == FAT_TYPE_16 ? 2U : 4U;
    fat_offset = cluster * entry_size;
    fat_sector = v->fat_begin_lba + fat_offset / FAT_SECTOR_SIZE;
    entry_offset = fat_offset % FAT_SECTOR_SIZE;

    r = sdmmc0_read_sector(fat_sector, sector);
    if (r)
        return r;

    if (v->fat_type == FAT_TYPE_16)
        *next = le16(&sector[entry_offset]);
    else
        *next = le32(&sector[entry_offset]) & 0x0FFFFFFFU;

    return 0;
}

static int parse_directory_sector(const uint8_t *sector,
                                  const char name83[11],
                                  uint32_t containing_lba,
                                  fat_file_t *file)
{
    unsigned offset;

    for (offset = 0U; offset < FAT_SECTOR_SIZE; offset += 32U) {
        const uint8_t *entry = &sector[offset];
        uint8_t first = entry[0];
        uint8_t attr = entry[11];
        uint32_t first_cluster;

        if (first == 0x00U)
            return 1;
        if (first == 0xE5U || attr == 0x0FU || (attr & 0x08U))
            continue;
        if (!name_equal_11(entry, name83))
            continue;

        first_cluster = ((uint32_t)le16(&entry[20]) << 16)
                      | le16(&entry[26]);
        if (first_cluster < 2U)
            return -2;

        file->first_cluster = first_cluster;
        file->first_sector_lba = containing_lba;
        file->size = le32(&entry[28]);
        file->attributes = attr;
        return 0;
    }

    return 2;
}

int fat_mount_first(fat_volume_t *v)
{
    static uint32_t sector_words[FAT_SECTOR_SIZE / 4U];
    uint8_t *sector = (uint8_t *)sector_words;
    uint32_t partition_lba = 0U;
    uint32_t partition_sectors = 0U;
    uint32_t reserved;
    uint32_t total_sectors;
    uint32_t fats_size;
    uint32_t root_dir_sectors;
    uint32_t non_data_sectors;
    uint32_t data_sectors;
    uint8_t partition_type = 0U;
    unsigned i;
    int r;

    if (!v)
        return -1;

    r = sdmmc0_read_sector(0U, sector);
    if (r)
        return -2;
    if (sector[510] != 0x55U || sector[511] != 0xAAU)
        return -3;

    for (i = 0U; i < 4U; i++) {
        const uint8_t *entry = &sector[MBR_PARTITION_TABLE
                                     + i * MBR_ENTRY_SIZE];
        uint32_t lba = le32(&entry[8]);
        uint32_t count = le32(&entry[12]);

        if (known_fat_partition(entry[4]) && lba && count) {
            partition_type = entry[4];
            partition_lba = lba;
            partition_sectors = count;
            break;
        }
    }

    if (!partition_lba || !partition_sectors)
        return -4;

    r = sdmmc0_read_sector(partition_lba, sector);
    if (r)
        return -5;
    if (sector[510] != 0x55U || sector[511] != 0xAAU)
        return -6;

    v->bytes_per_sector = le16(&sector[11]);
    v->sectors_per_cluster = sector[13];
    reserved = le16(&sector[14]);
    v->number_of_fats = sector[16];
    v->root_entry_count = le16(&sector[17]);
    total_sectors = le16(&sector[19]);
    if (!total_sectors)
        total_sectors = le32(&sector[32]);

    fats_size = le16(&sector[22]);
    if (!fats_size)
        fats_size = le32(&sector[36]);

    if (v->bytes_per_sector != FAT_SECTOR_SIZE)
        return -7;
    if (!v->sectors_per_cluster || !reserved ||
        !v->number_of_fats || !fats_size || !total_sectors)
        return -8;

    root_dir_sectors = ((uint32_t)v->root_entry_count * 32U
                       + FAT_SECTOR_SIZE - 1U) / FAT_SECTOR_SIZE;
    non_data_sectors = reserved
                     + (uint32_t)v->number_of_fats * fats_size
                     + root_dir_sectors;
    if (total_sectors <= non_data_sectors)
        return -9;

    data_sectors = total_sectors - non_data_sectors;
    v->total_clusters = data_sectors / v->sectors_per_cluster;

    /* Select layout from both BPB structure and computed cluster count. */
    if (v->root_entry_count != 0U && v->total_clusters < 65525U) {
        v->fat_type = FAT_TYPE_16;
        v->root_cluster = 0U;
    } else if (v->root_entry_count == 0U) {
        v->fat_type = FAT_TYPE_32;
        v->root_cluster = le32(&sector[44]);
        if (v->root_cluster < 2U)
            return -10;
    } else {
        return -11;
    }

    v->partition_lba = partition_lba;
    v->partition_sectors = partition_sectors;
    v->partition_type = partition_type;
    v->sectors_per_fat = fats_size;
    v->fat_begin_lba = partition_lba + reserved;
    v->root_dir_lba = v->fat_begin_lba
                    + (uint32_t)v->number_of_fats * fats_size;
    v->data_begin_lba = v->root_dir_lba + root_dir_sectors;

    return 0;
}

int fat_find_root(const fat_volume_t *v,
                  const char name83[11],
                  fat_file_t *file)
{
    static uint32_t sector_words[FAT_SECTOR_SIZE / 4U];
    uint8_t *sector = (uint8_t *)sector_words;
    uint32_t root_sectors;
    uint32_t cluster;
    uint32_t next;
    uint32_t lba;
    uint32_t hops = 0U;
    unsigned s;
    int parsed;
    int r;

    if (!v || !name83 || !file)
        return -1;

    if (v->fat_type == FAT_TYPE_16) {
        root_sectors = ((uint32_t)v->root_entry_count * 32U
                       + FAT_SECTOR_SIZE - 1U) / FAT_SECTOR_SIZE;

        for (s = 0U; s < root_sectors; s++) {
            lba = v->root_dir_lba + s;
            r = sdmmc0_read_sector(lba, sector);
            if (r)
                return -2;

            parsed = parse_directory_sector(sector, name83, lba, file);
            if (parsed == 0) {
                file->first_sector_lba = cluster_lba(v,
                                                     file->first_cluster);
                return 0;
            }
            if (parsed == 1)
                return -3;
            if (parsed < 0)
                return -4;
        }
        return -5;
    }

    if (v->fat_type != FAT_TYPE_32)
        return -6;

    cluster = v->root_cluster;
    while (cluster >= 2U && !cluster_is_eoc(v, cluster)) {
        lba = cluster_lba(v, cluster);
        for (s = 0U; s < v->sectors_per_cluster; s++) {
            r = sdmmc0_read_sector(lba + s, sector);
            if (r)
                return -7;

            parsed = parse_directory_sector(sector, name83, lba + s, file);
            if (parsed == 0) {
                file->first_sector_lba = cluster_lba(v,
                                                     file->first_cluster);
                return 0;
            }
            if (parsed == 1)
                return -8;
            if (parsed < 0)
                return -9;
        }

        r = next_cluster(v, cluster, &next);
        if (r)
            return -10;
        if (cluster_is_bad(v, next) || next < 2U)
            return -11;

        cluster = next;
        hops++;
        if (hops > v->total_clusters)
            return -12;
    }

    return -13;
}

int fat_read_file_sector(const fat_volume_t *v,
                         const fat_file_t *file,
                         uint32_t file_sector,
                         uint8_t *buffer)
{
    uint32_t cluster;
    uint32_t next;
    uint32_t cluster_index;
    uint32_t sector_in_cluster;
    uint32_t i;
    int r;

    if (!v || !file || !buffer)
        return -1;

    if (file_sector >= (file->size + FAT_SECTOR_SIZE - 1U)
                       / FAT_SECTOR_SIZE)
        return -2;

    cluster_index = file_sector / v->sectors_per_cluster;
    sector_in_cluster = file_sector % v->sectors_per_cluster;
    cluster = file->first_cluster;

    for (i = 0U; i < cluster_index; i++) {
        r = next_cluster(v, cluster, &next);
        if (r)
            return -3;
        if (next < 2U || cluster_is_eoc(v, next) ||
            cluster_is_bad(v, next))
            return -4;
        cluster = next;
    }

    return sdmmc0_read_sector(cluster_lba(v, cluster) + sector_in_cluster,
                              buffer);
}

int fat_file_reader_init(fat_file_reader_t *reader,
                         const fat_volume_t *volume,
                         const fat_file_t *file)
{
    if (!reader || !volume || !file)
        return -1;
    if (file->first_cluster < 2U || !volume->sectors_per_cluster)
        return -2;

    reader->volume = volume;
    reader->file = file;
    reader->current_cluster = file->first_cluster;
    reader->file_sector = 0U;
    reader->sector_in_cluster = 0U;
    reader->initialized = 1U;
    return 0;
}

int fat_file_reader_read_next(fat_file_reader_t *reader,
                              uint8_t *buffer)
{
    const fat_volume_t *v;
    uint32_t total_sectors;
    uint32_t next;
    int r;

    if (!reader || !buffer || !reader->initialized)
        return -1;

    v = reader->volume;
    total_sectors = (reader->file->size + FAT_SECTOR_SIZE - 1U)
                  / FAT_SECTOR_SIZE;
    if (reader->file_sector >= total_sectors)
        return -2;

    r = sdmmc0_read_sector(cluster_lba(v, reader->current_cluster)
                         + reader->sector_in_cluster,
                           buffer);
    if (r)
        return -3;

    reader->file_sector++;
    reader->sector_in_cluster++;

    if (reader->file_sector < total_sectors &&
        reader->sector_in_cluster >= v->sectors_per_cluster) {
        r = next_cluster(v, reader->current_cluster, &next);
        if (r)
            return -4;
        if (next < 2U || cluster_is_eoc(v, next) ||
            cluster_is_bad(v, next))
            return -5;

        reader->current_cluster = next;
        reader->sector_in_cluster = 0U;
    }

    return 0;
}
