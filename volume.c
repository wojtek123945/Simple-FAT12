#include "file_reader.h"

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector){
    if(!pdisk){
        errno = EFAULT;
        return NULL;
    }
    struct volume_t* new_volume = (calloc(1, sizeof(struct volume_t)));
    if(!new_volume){
        errno = ENOMEM;
        return NULL;
    }
    new_volume->disk = pdisk;
    struct fat_super_t* psuper = (calloc(1, sizeof(struct fat_super_t)));
    if(!psuper){
        free(new_volume);
        errno = ENOMEM;
        return NULL;
    }
    int error = disk_read(pdisk, (int32_t)first_sector, psuper, 1);
    if(error != 1){
        free(new_volume->super);
        free(new_volume);
        errno = ENOMEM;
        return NULL;
    }
    if(validate_super_fat(psuper) == -1){
        free(psuper);
        fat_close(new_volume);
        errno = EINVAL;
        return NULL;
    }

    new_volume->super = psuper;

    uint32_t total_sectors = 0;
    if(new_volume->super->logical_sectors16 == 0){
        total_sectors = new_volume->super->logical_sectors32;
    }else
        total_sectors = new_volume->super->logical_sectors16;
    ((void)total_sectors);

    lba_t fat_1 = first_sector + new_volume->super->reserved_sectors;
    lba_t fat_2 = first_sector + new_volume->super->reserved_sectors + new_volume->super->sectors_per_fat;
    //Check fat table
    uint8_t * fat_1_table = calloc(new_volume->super->sectors_per_fat * new_volume->super->bytes_per_sector, sizeof(uint8_t));
    uint8_t * fat_2_table = calloc(new_volume->super->sectors_per_fat * new_volume->super->bytes_per_sector, sizeof(uint8_t));

    disk_read(new_volume->disk, fat_1, fat_1_table, new_volume->super->sectors_per_fat);
    disk_read(new_volume->disk, fat_2, fat_2_table, new_volume->super->sectors_per_fat);

    if(memcmp(fat_1_table, fat_2_table, new_volume->super->sectors_per_fat * new_volume->super->bytes_per_sector) != 0){
        errno=EINVAL;
        free(new_volume->super);
        free(new_volume);
        free(fat_2_table);
        free(fat_1_table);
        return NULL;
    }

    lba_t dir_size = ((new_volume->super->root_dir_capacity * 32) + (new_volume->super->bytes_per_sector-1)) / new_volume->super->bytes_per_sector;
    if((new_volume->super->root_dir_capacity * 32) % psuper->bytes_per_sector != 0)
        dir_size++;

    new_volume->first_data_position = new_volume->super->reserved_sectors + (new_volume->super->fat_count * new_volume->super->sectors_per_fat) + dir_size;
    new_volume->root_dir_position = dir_size;

    free(fat_2_table);
    free(fat_1_table);


    return new_volume;
}
int validate_super_fat(struct fat_super_t* super){
    if(!super)
        return -1;
    uint8_t collection=1;
    int if_done = 0;
    for (int i = 0; i < 7; ++i) {
        if(super->sectors_per_cluster < collection) {
            collection <<= 1;
            continue;
        }
        if_done = 1;
        if(super->sectors_per_cluster != collection)
            return -1;
        else
            break;
    }
    if(if_done == 0)
        return -1;

    if(super->reserved_sectors <= 0 || ((super->logical_sectors16 == 0 && super->logical_sectors32 == 0) || (super->logical_sectors32 != 0 && super->logical_sectors16 != 0)))
        return -1;
    if(super->fat_count != 1 && super->fat_count != 2 || super->magic != 0xAA55)
        return -1;
    return 0;
}
int fat_close(struct volume_t* pvolume){
    if(!pvolume){
        errno = EFAULT;
        return -1;
    }
    if(pvolume->super)
        free(pvolume->super);
    free(pvolume);
    return 0;
}