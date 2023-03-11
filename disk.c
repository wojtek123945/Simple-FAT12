#include "file_reader.h"

struct disk_t* disk_open_from_file(const char* volume_file_name){
    if(!volume_file_name){
        errno = EFAULT;
        return NULL;
    }
    FILE* file = fopen(volume_file_name, "rb");
    if(!file){
        errno = ENOENT;
        return NULL;
    }
    struct disk_t* new_disk = (struct disk_t*)malloc(sizeof(struct disk_t));
    if(!new_disk){
        errno = ENOMEM;
        fclose(file);
        return NULL;
    }
    new_disk->file_disk_handler = file;
    return new_disk;
}
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read){
    if(!pdisk || !pdisk->file_disk_handler || !buffer || first_sector < 0){
        errno = EFAULT;
        return -1;
    }
    int error = fseek(pdisk->file_disk_handler, first_sector * BYTES_PER_SECTOR, SEEK_SET);

    (void)error;

    for(int i=0;i<sectors_to_read;i++)
    {
        if(fread((uint8_t*)buffer+(i*BYTES_PER_SECTOR),BYTES_PER_SECTOR,1,pdisk->file_disk_handler)!=1){
            errno = ERANGE;
            return -1;
        }
    }
    return sectors_to_read;
}
int disk_close(struct disk_t* pdisk){
    if(!pdisk){
        errno = EFAULT;
        return -1;
    }
    fclose(pdisk->file_disk_handler);
    free(pdisk);
    return 0;
}