#include "file_reader.h"

struct file_t* file_open(struct volume_t* pvolume, const char* file_name){
    if(!pvolume || !file_name){
        errno = EFAULT;
        return NULL;
    }

    struct file_t* new_file = calloc(sizeof(struct file_t), 1);
    if(!new_file){
        errno = ENOMEM;
        return NULL;
    }

    new_file->volume = pvolume;
    new_file->cluster = calloc(pvolume->super->sectors_per_cluster * pvolume->super->bytes_per_sector, sizeof(char));


    struct dir_t *root_catalog = dir_open(pvolume,"\\");
    struct dir_entry_t temp;

    while (1){
        int error = dir_read(root_catalog, &temp);
        if(error != 0){
            free(new_file->cluster);
            free(new_file);
            dir_close(root_catalog);
            return NULL;
        }
        if(strcmp(file_name, temp.name) == 0){
            break;
        }
    }
    if(temp.atribute.directory==1){
        free(new_file->cluster);
        free(new_file);
        dir_close(root_catalog);
        return NULL;
    }

    uint8_t *fat_table = calloc(new_file->volume->super->sectors_per_fat * new_file->volume->super->bytes_per_sector,
                                sizeof(uint8_t));

    disk_read(pvolume->disk,pvolume->super->reserved_sectors,fat_table,pvolume->super->sectors_per_fat);
    new_file->chain = get_chain_fat12(fat_table,pvolume->super->sectors_per_cluster*BYTES_PER_SECTOR, temp.lower_bits);

    disk_read(pvolume->disk,new_file->volume->first_data_position + (new_file->chain->clusters[0] - 2) * pvolume->super->sectors_per_cluster,new_file->cluster,pvolume->super->sectors_per_cluster);

    new_file->actual_cluster = 0;
    new_file->size = temp.size;

    free(fat_table);
    dir_close(root_catalog);
    return new_file;
}
int file_close(struct file_t* stream){
    if(!stream){
        errno = EFAULT;
        return -1;
    }
    if(stream->chain){
        if(stream->chain->clusters)
            free(stream->chain->clusters);
        free(stream->chain);
    }
    if(stream->cluster)
        free(stream->cluster);
    free(stream);
    return 0;
}
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream){
    if(!ptr || !stream){
        errno = EFAULT;
        return -1;
    }
    uint32_t bytes_in_cluster = stream->volume->super->bytes_per_sector * stream->volume->super->sectors_per_cluster;

    if(file_seek(stream, 0, SEEK_CUR) == -1){
        errno = ERANGE;
        return -1;
    }

    unsigned long load_data = 0;
    unsigned int count=stream->actual_byte%512;
    for (unsigned long i=0; i < size*nmemb; ++i, ++stream->count,++count, stream->actual_byte++, load_data++) {

        if((stream->actual_cluster * bytes_in_cluster + count) >= stream->size){
            break;
        }

        if(count >= bytes_in_cluster){
            if(file_seek(stream, 0, SEEK_CUR) == -1){
                errno = ERANGE;
                return -1;
            }
            count=0;
        }
        *((uint8_t*)ptr + i) = (uint8_t)*(stream->cluster + count);
        if((stream->actual_cluster * bytes_in_cluster + count) >= stream->size){
            break;
        }
    }

    return load_data/size;
}
int32_t file_seek(struct file_t* stream, int32_t offset, int whence){
    if(!stream){
        errno = EFAULT;
        return -1;
    }
    if(whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END){
        errno = EINVAL;
        return -1;
    }

    uint32_t bytes_in_cluster = stream->volume->super->sectors_per_cluster * stream->volume->super->bytes_per_sector;

    uint32_t index_cluster = offset/bytes_in_cluster;
    uint32_t byte_in_specific_cluster = offset % bytes_in_cluster;

    switch (whence) {
        case SEEK_END:{
            return file_seek(stream, offset + stream->size, SEEK_SET);
        }
        case SEEK_CUR:{
            uint32_t actual_position_in_file = stream->actual_cluster * bytes_in_cluster + stream->count;
            return file_seek(stream, actual_position_in_file + offset, SEEK_SET);
        }
        case SEEK_SET:{
            if((unsigned int)offset > stream->size){
                errno = ENXIO;
                return -1;
            }
            stream->actual_byte = stream->actual_cluster * bytes_in_cluster + stream->count;
            disk_read(stream->volume->disk, stream->volume->first_data_position+(stream->chain->clusters[index_cluster] - 2) * stream->volume->super->sectors_per_cluster, stream->cluster, stream->volume->super->sectors_per_cluster);
            break;
        }
    }

    stream->actual_cluster = index_cluster;
    stream->count = byte_in_specific_cluster;

    return stream->actual_byte;
}
struct clusters_chain_t *get_chain_fat12(const void * const buffer, size_t size, uint16_t first_cluster) {
    if (!buffer || size <= 0)
        return NULL;
    struct clusters_chain_t *new_cluster = calloc(1, sizeof(struct clusters_chain_t));
    if (!new_cluster)
        return NULL;
    new_cluster->size = 1;
    new_cluster->clusters = (uint16_t *) calloc(new_cluster->size, sizeof(uint16_t));
    if (!new_cluster->clusters) {
        free(new_cluster);
        return NULL;
    }

    uint32_t actual_byte_index = (first_cluster * 3) / 2;
    uint16_t actual_byte = *((uint8_t *) buffer + actual_byte_index);
    uint16_t actual_next_byte = *((uint8_t *) buffer + actual_byte_index + 1);

    uint16_t next_value_cluster = 0;
    new_cluster->clusters[new_cluster->size - 1] = first_cluster;
    if (first_cluster == 4088)
        return new_cluster;

    while (1) {
        next_value_cluster = 0;
        if (new_cluster->clusters[new_cluster->size - 1] % 2 == 0) {
            next_value_cluster = (actual_byte) | ((actual_next_byte & 0x000f) << 8);
        } else {
            next_value_cluster = (actual_byte >> 4) | (actual_next_byte << 4);;
        }
        if (next_value_cluster >= 0xff8)
            break;
        new_cluster->size++;
        uint16_t *new_tab = (uint16_t *) realloc(new_cluster->clusters, new_cluster->size * sizeof(uint16_t));
        if (!new_tab) {
            free(new_cluster->clusters);
            free(new_cluster);
            return NULL;
        }
        new_cluster->clusters = new_tab;
        new_cluster->clusters[new_cluster->size - 1] = next_value_cluster;

        actual_byte_index = (next_value_cluster * 3) / 2;
        actual_byte = (uint8_t) *((uint8_t *) buffer + actual_byte_index);
        actual_next_byte = (uint8_t) *((uint8_t *) buffer + actual_byte_index + 1);
    }
    return new_cluster;
}