#ifndef FILE_READER_H
#define FILE_READER_H
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "errno.h"
#include "string.h"

#define BYTES_PER_SECTOR 512

typedef uint32_t lba_t;

struct disk_t{
    FILE* file_disk_handler;
};
struct fat_super_t{
    uint8_t  jump_code[3];
    char oem_name[8];
    uint16_t  bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_dir_capacity;
    uint16_t logical_sectors16;
    uint8_t media_type;
    uint16_t sectors_per_fat;
    uint16_t chs_sectors_per_track;
    uint16_t chs_sectors_per_cylinder;
    uint32_t hidden_sectors;
    uint32_t logical_sectors32;
    uint8_t media_id;
    uint8_t chs_head;
    uint8_t ext_bpb_signature;
    uint32_t  serial_number;
    char volume_lable[11];
    char fsid[8];
    uint8_t boot_code[448];
    uint16_t magic; //Zawsze ma wartość "55AA"
}__attribute__(( packed ));
struct volume_t{
    struct disk_t * disk;
    struct fat_super_t *super;

    lba_t first_data_position;
    lba_t root_dir_position;
} __attribute__ (( packed ));

struct file_t{
    char *cluster;
    uint32_t count;
    uint32_t actual_byte;
    struct clusters_chain_t * chain;
    uint32_t actual_cluster;
    uint32_t size;
    struct volume_t *volume;
}__attribute__ (( packed ));

struct dir_t{
    char* catalog;
    int count;
    struct volume_t *dir_volume;
};

struct dir_atribute_t{
    uint8_t read_only_file:1;
    uint8_t hidden_file:1;
    uint8_t system_file:1;
    uint8_t volume_label:1;
    uint8_t directory:1;
    uint8_t archived:1;
    uint8_t long_file_name:1;
};

struct creation_data{
    uint8_t year:7;
    uint8_t  month:4;
    uint8_t day:5;
}__attribute__((packed));
struct creation_time{
    uint8_t hour:5;
    uint8_t minute:6;
    uint8_t second:5;
}__attribute__((packed));

struct dir_entry_t{
    char name [13];
    uint32_t size;
    struct dir_atribute_t atribute;
    uint16_t lower_bits;
}__attribute__ (( packed ));

struct dir_entry_t_extra{
    char name[8];
    char ext[3];
    struct dir_atribute_t dir_atribute;
    uint8_t reserved;
    uint8_t dir_time_ms;
    struct creation_time dir_time;
    struct creation_data dir_creation_data;
    struct creation_data dir_last_access_data;
    uint16_t first_clusters_high_bits;
    struct creation_time dir_last_mod_time;
    struct creation_data dir_last_mod_data;
    uint16_t first_clusters_low_bits;
    uint32_t size;
}__attribute__ (( packed ));

struct clusters_chain_t {
    uint16_t *clusters;
    size_t size;
};

struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);
int validate_super_fat(struct fat_super_t* super);

struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);

void set_dir_name(struct dir_entry_t* pentry,const char* name,const char* extenssion);
struct clusters_chain_t *get_chain_fat12(const void * const buffer, size_t size, uint16_t first_cluster);

#endif