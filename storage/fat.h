#ifndef FAT_H
#define FAT_H

#include <stdint.h>

#define FAT_SECTOR_SIZE 512U
#define FAT_TYPE_NONE   0U
#define FAT_TYPE_16     16U
#define FAT_TYPE_32     32U

typedef struct {
    uint32_t partition_lba;
    uint32_t partition_sectors;
    uint32_t fat_begin_lba;
    uint32_t root_dir_lba;
    uint32_t data_begin_lba;
    uint32_t root_cluster;
    uint32_t sectors_per_fat;
    uint32_t total_clusters;
    uint16_t bytes_per_sector;
    uint16_t root_entry_count;
    uint8_t sectors_per_cluster;
    uint8_t number_of_fats;
    uint8_t partition_type;
    uint8_t fat_type;
} fat_volume_t;

typedef struct {
    uint32_t first_cluster;
    uint32_t first_sector_lba;
    uint32_t size;
    uint8_t attributes;
} fat_file_t;

typedef struct {
    const fat_volume_t *volume;
    const fat_file_t *file;
    uint32_t current_cluster;
    uint32_t file_sector;
    uint32_t sector_in_cluster;
    uint8_t initialized;
} fat_file_reader_t;

int fat_mount_first(fat_volume_t *volume);
int fat_find_root(const fat_volume_t *volume,
                  const char name83[11],
                  fat_file_t *file);
int fat_read_file_sector(const fat_volume_t *volume,
                         const fat_file_t *file,
                         uint32_t file_sector,
                         uint8_t *buffer);

int fat_file_reader_init(fat_file_reader_t *reader,
                         const fat_volume_t *volume,
                         const fat_file_t *file);
int fat_file_reader_read_next(fat_file_reader_t *reader,
                              uint8_t *buffer);

#endif /* FAT_H */
