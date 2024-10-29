#include "fat32.h"
#include "disk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define OEM "INANGO\0"

#define MIN_DISKSIZE 1048576 // 1M =  1024*1024 2*20
#define DEFAULT_DISKSIZE 20971520 //20M = 1024 * 1024 * 20. Used when pathname for disk does not exist 
#define MAX_DISKSIZE 268435456 // 256M 2**28

int cd(BootSector* vbs, FSinfo *fsinfo);
int ls(Disk* disk, BootSector* vbs, FSinfo *fsinfo);
int touch(Disk* disk, BootSector* vbs, FSinfo *fsinfo, char *filename);
void create_file(char *filename, int isDir, DirectoryEntry* dir_entry);
int allocate_cluster(Disk* disk, BootSector* vbs, FSinfo *fsinfo);


//int mkdir(BootSector* vbs, FSinfo *fsinfo);

int allocate_cluster(Disk *disk, BootSector *vbs, FSinfo *fsinfo){
    
    FILE *fp;
    Fat32Entry fat32entry;

    fp = fopen(disk->absolute_path, "r+");
    fseek(fp, disk->fat_area, SEEK_SET);

    int fat32len = vbs->sectors_per_fat32 * vbs->bytes_per_sector;
    for (
            int i = 0, j = 0; i < fat32len;
            i += 4, j++
        )
        {
        fread(&fat32entry, 4, 1, fp);
        if ((fat32entry.mark & 0xFFFFFFFF) == 0){
            fat32entry.mark = EOC_BLOCK;
            fseek(fp, disk->fat_area + (j * 4), SEEK_SET);
            fwrite(&fat32entry, sizeof(fat32entry), 1, fp);
            printf("new allocated cluster number: %d\n", j);
            fclose(fp);
            return j;
        }
    }
    fclose(fp);
    return -1;
}


int open_disk(char *pathname, Disk *disk){
    
    struct stat sb;
    size_t pathlen;

    /* Check if file exist, otherwise create it */

    if (access(pathname, F_OK) != 0){
        if (create_disk(pathname) != 0){
            printf("creation error\n");
            return -1;
        }
    }

    if (stat(pathname, &sb)!= 0){
        printf("some stat error\n");
        return -1;
    }
    
    printf("file path: %s\tsize: %lu\n", pathname, sb.st_size);

    if (sb.st_size >= MAX_DISKSIZE || sb.st_size <= MIN_DISKSIZE){
        printf("Specified file is too smal to be valid disk file\n");
        return -1;
    }
    
    pathlen = strlen(pathname);
    
    disk->disk_size = sb.st_size;
    disk->absolute_path = malloc(pathlen);
    strncpy(disk->absolute_path, pathname, pathlen);
    return 0;
}


int create_disk(char *pathname){
    FILE *fp;
    
    fp = fopen(pathname, "w");
    if ( fp == NULL ){
        return -1;
    }
    if (fseek(fp, DEFAULT_DISKSIZE, SEEK_SET) == -1){
        return -1;
    }
    fputc('\0', fp);
    fclose(fp);
    return 0;
}


/* int extract_dir_entry(char *buf32, DirectoryEntry* dir_entry){
    dir_entry = (DirectoryEntry *) buf32;
} */

int wipe_disk(char *pathname, size_t disk_size){

    FILE *fp;
    int status;

    fp = fopen(pathname, "w");

    if (fp == NULL){
        printf("error while opening file\n");
        return -1;
    }
    for (int i = 0; i < disk_size; i++){
        status = fputc('\0', fp);
        if (status == -1){
            if (feof(fp)){
                printf("EOF detected. Some strange shit is going on\n");
            } else {
                printf("Some error happend. Aborting\n");
            }
            return -1;
        }
    }
    printf("disk file successfully wiped! Congrats\n");
    fclose(fp);
    return 0;
}


int ls(Disk *disk, BootSector *vbs, FSinfo *fsinfo){
    
    FILE *fp;
    int read_block;
    char buf[32];
    DirectoryEntry *dir_entry;

    read_block = vbs->bytes_per_sector * vbs->sector_per_cluster; //2048
    
    fp = fopen(disk->absolute_path, "r+");
    fseek(fp, disk->data_area, SEEK_SET);
    

    while(1){
        for (int i = 0; i < 128; i += disk->entry_offset){
            fread(buf, disk->entry_offset, 1, fp);
            dir_entry = (DirectoryEntry *) buf;
            printf("file name: %s\t%d\t", dir_entry->directory_name, dir_entry->directory_name[0]);
            printf("start cluster: %d\n", dir_entry->first_cluster_lower);
        }
        break;
    }
    return 0;
}

void create_file(char* filename, int isDir, DirectoryEntry *dir_entry){
    
    time_t now;
    int _date, _time, _year, _mon,
    _day, _hour, _min, _sec;

    now = time(NULL);
    struct tm *tm;
    
    if ((tm = localtime(&now)) == NULL) {
        printf ("Error extracting time stuff\n");
        return;
    }
    tm->tm_mon += 1;
    
    /* 
        Bits 0–4: Day of month, valid value range 1-31 inclusive.
        Bits 5–8: Month of year, 1 = January, valid value range 1–12 inclusive.
        Bits 9–15: Count of years from 1980 
    */
    /* because Year	- 1900))))  */
    
    _year = (tm->tm_year - 80)  & 0xFF ;
    _mon = tm->tm_mon & 0xF;
    _day = tm->tm_mday & 0x1F;
    //printf("year: %d, mon: %d, day: %d\n", _year, _mon, _day);

    /* 
        Bits 0–4: 2-second count, valid value range 0–29 inclusive (0 – 58 seconds).
        Bits 5–10: Minutes, valid value range 0–59 inclusive.
        Bits 11–15: Hours, valid value range 0–23 inclusive.
    */
    
    _hour = tm->tm_hour;
    _min = tm->tm_min;
    _sec = tm->tm_sec;

    _date = _year << 9 | _mon << 5 | _day; 
    _time = _hour << 10 | _min << 5 | _sec;

    strncpy(dir_entry->directory_name, filename, 11);    
    dir_entry->creation_time = _time;
    dir_entry->creation_date = _date;
    dir_entry->last_access_date = _date;
    dir_entry->write_date = _date;
    dir_entry->write_time = _time;
    dir_entry->file_size = 35;
    return;
}

int touch(Disk *disk, BootSector *vbs, FSinfo *fsinfo, char *filename){
    
    FILE *fp;
    int read_block, cluster_number;
    DirectoryEntry dir_entry;

    read_block = vbs->bytes_per_sector * vbs->sector_per_cluster;
    fp = fopen(disk->absolute_path, "r+");
    fseek(fp, disk->data_area, SEEK_SET);

    while(1){
        for (int i = 0; i < read_block; i += disk->entry_offset){
            fread(&dir_entry, disk->entry_offset, 1, fp);
            if (dir_entry.creation_time == 0){
                create_file(filename, 0, &dir_entry);
                cluster_number = allocate_cluster(disk, vbs, fsinfo);

                dir_entry.first_cluster_high = (cluster_number & 0xFFFF0000) >> 16;
                dir_entry.first_cluster_lower = cluster_number & 0x0000FFFF;
                
                fseek(fp, disk->data_area + i, SEEK_SET);
                fwrite(&dir_entry, sizeof(dir_entry), 1, fp);
                fseek(fp, disk->data_area, SEEK_SET);
                break;
            }
        }
        break;
    }
    return 0;
}

int format_disk(Disk *disk, BootSector* vbs, FSinfo *fsinfo){
    
    Fat32Entry f32entry;

    FILE *fp;
    
    int fat32_sectors, total_sectors, faf32offset;
    
    fat32_sectors = get_optimal_FAT32sectors(disk->disk_size);
    total_sectors = disk->disk_size / 512;
    
    // VOLUME BOOT SECTOR
    memset(vbs->boot_jump_instruction, '\0', 3);
    memcpy(vbs->OEM_identifier, OEM, sizeof(OEM));
    vbs->bytes_per_sector   = 512;
    vbs->sector_per_cluster = 4;
    vbs->reserved_sectors   = 32; // 8 clusters
    vbs->fat_count          = 2;
    vbs->root_enrtry_count  = 0;
    vbs->total_sectors16    = 0;
    vbs->media_descriptor   = 0xF8; // fixed (non removable) 0xF0 for removable (it anyway in my emu dont make sense so who cares)
    vbs->sectors_per_fat16  = 0;
    vbs->sectors_per_track  = 0;
    vbs->number_of_heads    = 0;
    vbs->hidden_sectors     = 0; // number of sectors before volume boot sector. In our case it will always be 0
    vbs->total_sectors32    = total_sectors;
    vbs->sectors_per_fat32  = fat32_sectors;
    vbs->ext_flags          = 0;
    vbs->file_system_version = 0;
    vbs->root_cluster_number = 2;
    vbs->FSinfo              = 1;
    vbs->backup_boot_sector  = 6;
    memset(vbs->reserved1, '\0', 12);
    vbs->drive_number        = 0;
    vbs->reserved2           = 0;
    vbs->boot_signature      = 0x29;
    vbs->serial_number       = 12345;
    memcpy(vbs->volume_label, "NO NAME    ", 11);
    memcpy(vbs->file_system_type, "FAT32   ", 8);
    memset(vbs->boot_code, '\0', 420);
    vbs->trial_signature     =  0xAA55;


    // FSINFO
    fsinfo->lead_signature = 0x41615252;
    memset(fsinfo->reserved1, '\0', 480);
    fsinfo->struct_signature = 0x61417272;
    fsinfo->free_clusters =  0xFFFFFFFF;
    fsinfo->next_free =  0xFFFFFFFF;
    memset(fsinfo->reserved2, '\0', 12);
    fsinfo->trial_signature = 0xAA550000;

    
    printf("volume BootSector size: %lu\n", sizeof(vbs));

    if (wipe_disk(disk->absolute_path, disk->disk_size) != 0){
        return -1;
    }

    fp = fopen(disk->absolute_path, "r+");
    // RESERVED AREA
    fwrite(vbs, sizeof(*vbs), 1, fp);
    fseek(fp, (vbs->FSinfo * vbs->bytes_per_sector), SEEK_SET);
    fwrite(fsinfo, sizeof(*fsinfo), 1, fp);
    fseek(fp, (vbs->backup_boot_sector * vbs->bytes_per_sector), SEEK_SET);
    fwrite(vbs, sizeof(*vbs), 1, fp);           // make a volume boot sector backup
    fwrite(fsinfo, sizeof(*fsinfo), 1, fp);     // and add fsinfo backup also

    // FAT32 AREA
    fseek(fp, (vbs->reserved_sectors  * vbs->bytes_per_sector), SEEK_SET);
    f32entry.mark = (vbs->media_descriptor ^ 0xFFFFFFFF) ^ 0xFF;
    fwrite(&f32entry, sizeof(f32entry), 1, fp);
    f32entry.mark = (HardErrBitMask | CleanShutBitMask ) ^ 0xFFFFFFFF ^ 0xC0000000;
    fwrite(&f32entry, sizeof(f32entry), 1, fp);
    f32entry.mark = EOC_BLOCK;
    fwrite(&f32entry, sizeof(f32entry), 1, fp);
    fclose(fp);



    disk->reserved_area = 0;
    disk->fat_area      = vbs->reserved_sectors * vbs->bytes_per_sector;
    disk->data_area     = disk->fat_area + (vbs->sectors_per_fat32 * 512 
                            * vbs->fat_count) + vbs->root_cluster_number * 512 * 4;
    disk->entry_offset = 32;

    
}


int get_optimal_FAT32sectors(size_t disk_size){
    /*     
    1 fat32 entry = 32 bit
    1 entry = 1 cluster number (512*4)
    so we first get count of entries that we have 2 aquire disk_size / (512 * 4)
    and then multypling it for fat entry size and convert to sectors instead of bytes
    */
    return (disk_size / (512 * 4) * 32) / 512;
}


void loop(Disk *disk){
    
    BootSector vbs;
    FSinfo fsinfo;
    DirectoryEntry* dir;

    //create_file("sperma", 1, dir, disk, vbs, fsinfo);
    format_disk(disk, &vbs, &fsinfo);
    ls(disk, &vbs, &fsinfo);
    touch(disk, &vbs, &fsinfo, "testfi");
    ls(disk, &vbs, &fsinfo);
    touch(disk, &vbs, &fsinfo, "testfile2");
    ls(disk, &vbs, &fsinfo);
}

int main(int argc, char *argv[])
{
    if (argc != 2){
        printf("wrong usage. <command> <path to disk file>\n");
        return -1;
    }
    
    char *pathname = argv[1];
    Disk disk;
    if (open_disk(pathname, &disk) == 0){
        loop(&disk);
    }
    return 0;
}



/* 

0xFFFFFFFF

0x000000F8

1111 1111 1111 1111     1111 1111 1111 1111
0000 0000 0000 0000     1111 1111 1000 0000

XOR

1111 1111 1111 1111     0000 0000 0111 1111
                        XOR
                        1111 1111 1111 1111 


1111 1111 1111 1111     1111 1111 1000 0000

0xFFFFFFF8


*/