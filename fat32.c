#include "fat32.h"
#include "disk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define OEM "INANGO\0"

#define MIN_DISKSIZE 1048576 // 1M =  1024*1024 2*20
#define DEFAULT_DISKSIZE 20971520 //20M = 1024 * 1024 * 20. Used when pathname for disk does not exist 
#define MAX_DISKSIZE 268435456 // 256M 2**28


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


int wipe_disk(char *pathname, size_t disk_size){

    FILE *fp;
    int status;

    fp = fopen(pathname, "w");

    if (fp == NULL){
        printf("error while opening file");
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


int format_disk(Disk *disk){
    
    BootSector vbs;
    FSinfo fsinfo;
    Fat32Entry f32entry;

    FILE *fp;
    
    int fat32_sectors, total_sectors, faf32offset;
    
    fat32_sectors = get_optimal_FAT32sectors(disk->disk_size);
    total_sectors = disk->disk_size / 512;
    
    // VOLUME BOOT SECTOR
    memset(vbs.boot_jump_instruction, '\0', 3);
    memcpy(vbs.OEM_identifier, OEM, sizeof(OEM));
    vbs.bytes_per_sector   = 512;
    vbs.sector_per_cluster = 4;
    vbs.reserved_sectors   = 32; // 8 clusters
    vbs.fat_count          = 2;
    vbs.root_enrtry_count  = 0;
    vbs.total_sectors16    = 0;
    vbs.media_descriptor   = 0xF8; // fixed (non removable) 0xF0 for removable (it anyway in my emu dont make sense so who cares)
    vbs.sectors_per_fat16  = 0;
    vbs.sectors_per_track  = 0;
    vbs.number_of_heads    = 0;
    vbs.hidden_sectors     = 0; // number of sectors before volume boot sector. In our case it will always be 0
    vbs.total_sectors32    = total_sectors;
    vbs.sectors_per_fat32  = fat32_sectors;
    vbs.ext_flags          = 0;
    vbs.file_system_version = 0;
    vbs.root_cluster_number = 2;
    vbs.FSinfo              = 1;
    vbs.backup_boot_sector  = 6;
    memset(vbs.reserved1, '\0', 12);
    vbs.drive_number        = 0;
    vbs.reserved2           = 0;
    vbs.boot_signature      = 0x29;
    vbs.serial_number       = 12345;
    memcpy(vbs.volume_label, "NO NAME    ", 11);
    memcpy(vbs.file_system_type, "FAT32   ", 8);
    memset(vbs.boot_code, '\0', 420);
    vbs.trial_signature     =  0xAA55;

    // FSINFO
    fsinfo.lead_signature = 0x41615252;
    memset(fsinfo.reserved1, '\0', 480);
    fsinfo.struct_signature = 0x61417272;
    fsinfo.free_clusters =  0xFFFFFFFF;
    fsinfo.next_free =  0xFFFFFFFF;
    memset(fsinfo.reserved2, '\0', 12);
    fsinfo.trial_signature = 0xAA550000;

    
    printf("volume BootSector size: %lu", sizeof(vbs));

    //if (wipe_disk(disk->absolute_path, disk->disk_size) != 0){
    //    return -1;
    //}

    fp = fopen(disk->absolute_path, "w");
    
    // RESERVED AREA
    fwrite(&vbs, sizeof(vbs), 1, fp);
    fseek(fp, (vbs.FSinfo * vbs.bytes_per_sector), SEEK_SET);
    fwrite(&fsinfo, sizeof(fsinfo), 1, fp);
    fseek(fp, (vbs.backup_boot_sector * vbs.bytes_per_sector), SEEK_SET);
    fwrite(&vbs, sizeof(vbs), 1, fp);           // make a volume boot sector backup
    fwrite(&fsinfo, sizeof(fsinfo), 1, fp);     // and add fsinfo backup also
    fseek(fp, (vbs.reserved_sectors  * vbs.bytes_per_sector), SEEK_SET);

    // FAT32 AREA
    f32entry.mark = vbs.media_descriptor | 0x11111111;
    fwrite(&f32entry, sizeof(f32entry), 1, fp);
    f32entry.mark = (HardErrBitMask & CleanShutBitMask) | 0x1111111;
    fwrite(&f32entry, sizeof(f32entry), 1, fp);
    fclose(fp);
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






int cd();
int ls();
int touch();
int mkdir();


void loop(Disk *disk){

    format_disk(disk);
    

    //while(1){}

    /* 
    
        Some interactive command shit

    */

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