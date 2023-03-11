#include "file_reader.h"

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path){
    if(!pvolume || !dir_path){
        errno = EFAULT;
        return NULL;
    }
    if(strcmp(dir_path,"\\") != 0){
        errno = ENOENT ;
        return NULL;
    }
    struct dir_t* new_dir_t = calloc(1, sizeof(struct dir_t));
    if(!new_dir_t){
        errno = ENOMEM  ;
        return NULL;
    }

    new_dir_t->catalog = calloc(pvolume->super->root_dir_capacity * pvolume->super->bytes_per_sector, sizeof(char));
    if(!new_dir_t->catalog){
        errno = ENOMEM;
        free(new_dir_t);
        return NULL;
    }
    disk_read(pvolume->disk,pvolume->first_data_position - pvolume->root_dir_position,(void*)new_dir_t->catalog,pvolume->super->root_dir_capacity);
    new_dir_t->dir_volume = pvolume;
    return new_dir_t;
}
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry){
    if(!pdir || !pentry){
        errno = EFAULT;
        return -1;
    }
    struct dir_entry_t_extra* dir_extra;

    while (1){
        dir_extra = (struct dir_entry_t_extra *) ((char *) pdir->catalog + pdir->count * sizeof(struct dir_entry_t_extra));
        pdir->count++;
        if(*(uint8_t*)dir_extra->name < 0xe5)
            break;
    }
    if(*(uint8_t*)dir_extra->name == 0x00){
        errno = EFAULT;
        return 1;
    }

    if(*(uint8_t*)dir_extra->name == 0)
    {
        return 1;
    }

    memset(pentry->name, '\0', 13);
    set_dir_name(pentry, dir_extra->name, dir_extra->ext);

    pentry->size = dir_extra->size;
    memcpy(&pentry->atribute, &dir_extra->dir_atribute, sizeof(struct dir_atribute_t));
    pentry->lower_bits = dir_extra->first_clusters_low_bits;

    return 0;
}

void set_dir_name(struct dir_entry_t* pentry,const char* name,const char* extenssion){
    int i=0;
    int x=0;
    for (; i < 8; ++i) {
        if(name[i] != ' ') {
            pentry->name[i] = name[i];
            x++;
        }
        else
            continue;
    }
    if(*extenssion == ' '){
        return;
    }
    pentry->name[x] = '.';

    for (int z = 0; i < 11; ++i, ++z, x++) {
        if(name[i] != ' ')
            pentry->name[x + 1] = extenssion[z];
        else
            continue;
    }
    pentry->name[i+1] = '\0';
}

int dir_close(struct dir_t* pdir){
    if(!pdir){
        errno = EFAULT;
        return -1;
    }
    if(pdir->catalog)
        free(pdir->catalog);
    free(pdir);
    return 0;
}