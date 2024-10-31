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

int cd(char *pathname);
int ls(void);
int touch(char *filename);
void create_file(char *filename, int isDir, DirectoryEntry* dir_entry);
int allocate_cluster(void);
int pathfinder(char *filename);
int my_mkdir(char *filename);

Disk g_disk;
BootSector g_vbs;
FSinfo g_fsinfo;
FILE *fp;


int open_disk(char *pathname){
    
    struct stat sb;
    size_t pathlen;


    /* Check if file exist, otherwise create it */

    if (access(pathname, F_OK) != 0){
        if (create_disk(pathname) != 0){
            printf("creation error\n");
            return -1;
        }
    } else {
        fp = fopen(pathname, "r+");
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

    g_disk.disk_size = sb.st_size;
    g_disk.absolute_path = malloc(pathlen);
    strncpy(g_disk.absolute_path, pathname, pathlen);
    return 0;
}


int create_disk(char *pathname){
    

    fp = fopen(pathname, "w+");
    
    if ( fp == NULL ){
        return -1;
    }
    if (fseek(fp, DEFAULT_DISKSIZE, SEEK_SET) == -1){
        return -1;
    }

    fputc('\0', fp);
    fseek(fp, 0, SEEK_SET);

    return 0;
}


int wipe_disk(char *pathname, size_t disk_size){

    int status;


    fseek(fp, g_disk.reserved_area, SEEK_SET);

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
    return 0;
}

/*====================================
WORK WITH FILE SYSTEM RAHTER THAN DISK
======================================*/
int format_disk(){
    
    Fat32Entry f32entry;
    int fat32_sectors, total_sectors, faf32offset;
    
    fseek(fp, 0, SEEK_SET);
    fat32_sectors = get_optimal_FAT32sectors(g_disk.disk_size);
    total_sectors = g_disk.disk_size / 512;
    

    // VOLUME BOOT SECTOR
    memset(g_vbs.boot_jump_instruction, '\0', 3);
    memcpy(g_vbs.OEM_identifier, OEM, sizeof(OEM));
    g_vbs.bytes_per_sector   = 512;
    g_vbs.sector_per_cluster = 4;
    g_vbs.reserved_sectors   = 32; // 8 clusters
    g_vbs.fat_count          = 2;
    g_vbs.root_enrtry_count  = 0;
    g_vbs.total_sectors16    = 0;
    g_vbs.media_descriptor   = 0xF8; // fixed (non removable) 0xF0 for removable (it anyway in my emu dont make sense so who cares)
    g_vbs.sectors_per_fat16  = 0;
    g_vbs.sectors_per_track  = 0;
    g_vbs.number_of_heads    = 0;
    g_vbs.hidden_sectors     = 0; // number of sectors before volume boot sector. In our case it will always be 0
    g_vbs.total_sectors32    = total_sectors;
    g_vbs.sectors_per_fat32  = fat32_sectors;
    g_vbs.ext_flags          = 0;
    g_vbs.file_system_version = 0;
    g_vbs.root_cluster_number = 2;
    g_vbs.FSinfo              = 1;
    g_vbs.backup_boot_sector  = 6;
    memset(g_vbs.reserved1, '\0', 12);
    g_vbs.drive_number        = 0;
    g_vbs.reserved2           = 0;
    g_vbs.boot_signature      = 0x29;
    g_vbs.serial_number       = 12345;
    memcpy(g_vbs.volume_label, "NO NAME    ", 11);
    memcpy(g_vbs.file_system_type, "FAT32   ", 8);
    memset(g_vbs.boot_code, '\0', 420);
    g_vbs.trial_signature     =  0xAA55;


    // FSINFO
    g_fsinfo.lead_signature = 0x41615252;
    memset(g_fsinfo.reserved1, '\0', 480);
    g_fsinfo.struct_signature = 0x61417272;
    g_fsinfo.free_clusters =  0xFFFFFFFF;
    g_fsinfo.next_free =  0xFFFFFFFF;
    memset(g_fsinfo.reserved2, '\0', 12);
    g_fsinfo.trial_signature = 0xAA550000;

    
    printf("volume BootSector size: %lu\n", sizeof(g_vbs));

    if (wipe_disk(g_disk.absolute_path, g_disk.disk_size) != 0){
        return -1;
    }

    g_disk.reserved_area = 0;
    g_disk.fat_area      = g_vbs.reserved_sectors * g_vbs.bytes_per_sector;
    g_disk.data_area     = g_disk.fat_area + (g_vbs.sectors_per_fat32 * 512 
                            * g_vbs.fat_count) + g_vbs.root_cluster_number * 512 * 4;
    g_disk.data_offset = 32;
    g_disk.fat32_offset = 4;
    g_disk.current_directory = g_disk.data_area;


    fseek(fp, g_disk.reserved_area, SEEK_SET);
    // RESERVED AREA
    fwrite(&g_vbs, sizeof(g_vbs), 1, fp);
    fseek(fp, (g_vbs.FSinfo * g_vbs.bytes_per_sector), SEEK_SET);
    fwrite(&g_fsinfo, sizeof(g_fsinfo), 1, fp);
    fseek(fp, (g_vbs.backup_boot_sector * g_vbs.bytes_per_sector), SEEK_SET);
    fwrite(&g_vbs, sizeof(g_vbs), 1, fp);           // make a volume boot sector backup
    fwrite(&g_fsinfo, sizeof(g_fsinfo), 1, fp);     // and add fsinfo backup also

    // FAT32 AREA
    fseek(fp, (g_vbs.reserved_sectors  * g_vbs.bytes_per_sector), SEEK_SET);
    f32entry.mark = (g_vbs.media_descriptor ^ 0xFFFFFFFF) ^ 0xFF;
    fwrite(&f32entry, sizeof(f32entry), 1, fp);
    f32entry.mark = (HardErrBitMask | CleanShutBitMask ) ^ 0xFFFFFFFF ^ 0xC0000000;
    fwrite(&f32entry, sizeof(f32entry), 1, fp);
    f32entry.mark = EOC_BLOCK;
    fwrite(&f32entry, sizeof(f32entry), 1, fp);
    
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

// main purpose is to find free block in FAT32 area and mark it is EOC. NOTHING MORE!
int allocate_cluster(){
    
    Fat32Entry fat32entry;

    fseek(fp, g_disk.fat_area, SEEK_SET);

    int fat32len = g_vbs.sectors_per_fat32 * g_vbs.bytes_per_sector;

    // 1 ENTRY = 32 BITS = 4 BYTES 
    // walking through fat32 table to find unused cluster and marks it as EOC
    for (
            int i = 0, j = 0; i < fat32len;
            i += 4, j++
        )
        {
        fread(&fat32entry, 4, 1, fp);
        if ((fat32entry.mark & 0xFFFFFFFF) == 0){
            fat32entry.mark = EOC_BLOCK;
            fseek(fp, g_disk.fat_area + (j * 4), SEEK_SET);
            fwrite(&fat32entry, sizeof(fat32entry), 1, fp);
            printf("new allocated cluster number: %d\n", j);
            return j;
        }
    }
    return -1;
}


int pathfinder(char *filepath){
    
    char *token;
    uint32_t start_directory;
    DirectoryEntry dir_entry;

    int read_block = g_vbs.sector_per_cluster * g_vbs.bytes_per_sector;

    if (filepath[0] == '\\'){
        start_directory = g_disk.data_area;
    } else {
        start_directory = g_disk.current_directory;
    }

    fseek(fp, start_directory, SEEK_SET);
    token = strtok(filepath, "\\");    

    while(token != NULL){
        for (int i = 0; i < read_block; i += g_disk.data_offset){
            fread(&dir_entry, sizeof(dir_entry), 1, fp);
            if (strncmp(token, dir_entry.directory_name, 11) == 0){
                return i + start_directory;
            }
        }
        token = strtok(NULL, "\\");
    }
    return -1;
}


int ls(){
    
    int read_block;
    DirectoryEntry dir_entry;

    read_block = g_vbs.bytes_per_sector * g_vbs.sector_per_cluster; //2048    
    fseek(fp, g_disk.data_area, SEEK_SET);
    
    while(1){
        for (int i = 0; i < read_block; i += g_disk.data_offset){
            fread(&dir_entry, g_disk.data_offset, 1, fp);
            if ((dir_entry.creation_date | 0x00000000) == 0){
                return 0;
            }
            if (dir_entry.directory_attributes & ATTR_HIDDEN){
                continue;
            }
            printf("file name: %s\tstart cluster: %d\n", dir_entry.directory_name,
                            dir_entry.first_cluster_lower);
        }
        break;
    }
    return 0;
}





int cd(char *pathname){
    return 1;
}


void create_file(char* filename, int isDir, DirectoryEntry *dir_entry){
    
    time_t now;
    int _date, _time, _year, _mon,
    _day, _hour, _min, _sec;
    int cluster_number;
    int attributes = 0;

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
    // allocate new memory in fat32 table and returns new cluster number
    cluster_number = allocate_cluster();

    // fill cluster number in struct
    dir_entry->first_cluster_high = (cluster_number & 0xFFFF0000) >> 16;
    dir_entry->first_cluster_lower = cluster_number & 0x0000FFFF;

    if (filename[0] == '.'){
        attributes |= ATTR_HIDDEN;
    }
    if (isDir == 0){
        attributes |= ATTR_DIRECTORY;
    }

    dir_entry->directory_attributes = attributes;

    return;
}


int touch(char *filename){
    
    int read_block, cluster_number;
    DirectoryEntry dir_entry;

    read_block = g_vbs.bytes_per_sector * g_vbs.sector_per_cluster;
    fseek(fp, g_disk.data_area, SEEK_SET);

    while(1){
        for (int i = 0; i < read_block; i += g_disk.data_offset){
            
            // read each entry to find free space to plase new direcotry entry strcuture
            fread(&dir_entry, g_disk.data_offset, 1, fp);
            if (dir_entry.creation_time == 0){

                // fill dir_entry struct with meta information
                create_file(filename, 0, &dir_entry);
                fseek(fp, g_disk.data_area + i, SEEK_SET);
                //write new dir entry inside directory
                fwrite(&dir_entry, sizeof(dir_entry), 1, fp);
                break;
            }
        }
        break;
    }
    return 0;
}


int my_mkdir(char *filename){
    return 1;
}



void loop(){
    
    BootSector vbs;
    FSinfo fsinfo;
    DirectoryEntry* dir;

    format_disk();
    ls();

    if(pathfinder("sperma5") != -1){
        printf("file sperma exists\n");
    } else {
        printf("file sperma does not exists\n");
    }
    touch("sperma");
    touch("sperma2");
    touch("sperma5");
    if(pathfinder("sperma5") != -1){
        printf("file sperma exists\n");
    } else {
        printf("file sperma does not exists\n");
    }
    touch(".Dimaloot");
    touch("LootIn)");
    ls();
}

int main(int argc, char *argv[])
{
    if (argc != 2){
        printf("wrong usage. <command> <path to disk file>\n");
        return -1;
    }
    
    char *pathname = argv[1];
    
    if (open_disk(pathname) == 0){
        loop(fp);
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