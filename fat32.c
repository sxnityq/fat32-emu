#include "fat32.h"
#include "disk.h"
#include "commands.h"
#include "collors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>


#define MAX_TOKENS      20
#define MAX_COMMAND_LEN 256

#define TRUE            1
#define FALSE           0

int format_disk();
void create_file(char *filename, int isDir, DirectoryEntry* dir_entry);
int allocate_cluster(void);
int pathfinder(char *filename, DirectoryEntry *);
char **parse_tokens(char *, int *);


Disk g_disk;
BootSector g_vbs;
FSinfo g_fsinfo;
FILE *fp;


DirectoryEntry root_entry;

void slice(const char* str, char* result, size_t start, size_t end) {
    strncpy(result, str + start, end - start);
}


int open_disk(char *pathname){
    
    struct stat sb;
    size_t pathlen;

    /* Check if file exist, otherwise create it */

    if (access(pathname, F_OK) != 0){
        if (create_disk(pathname) != 0){
            printf("%s%s%s", 
            RED, "creation error\n", ENDCOLOR);
            return -1;
        }
    } else {
        fp = fopen(pathname, "r+");
    }

    if (stat(pathname, &sb)!= 0){
        printf("%s%s%s", 
        RED, "some stat error\n", ENDCOLOR);
        return -1;
    }
    
    printf("%sdisk file: %s\tsize: %lu%s\n", 
    BLUE, pathname, sb.st_size, ENDCOLOR);

    if (sb.st_size >= MAX_DISKSIZE || sb.st_size <= MIN_DISKSIZE){
        printf("Specified file is too smal to be valid disk file\n");
        return -1;
    }
    
    pathlen = strlen(pathname);

    g_disk.disk_size = sb.st_size;
    g_disk.absolute_path = malloc(pathlen);
    strncpy(g_disk.absolute_path, pathname, pathlen);
    fill_disk();
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


int wipe_disk(size_t disk_size){

    int status;

    fseek(fp, g_disk.reserved_area, SEEK_SET);

    for (int i = 0; i < disk_size; i++){
        status = fputc('\0', fp);
        if (status == -1){
            if (feof(fp)){
                printf("%s%s%s", 
                RED, "EOF detected. Some strange shit is going on\n", ENDCOLOR);
            } else {
                printf("%s%s%s", 
                RED, "Some error happend. Aborting\n", ENDCOLOR);
            }
            return -1;
        }
    }
    printf("%s%s%s",
    BLUE, "disk file successfully wiped! Congrats\n", ENDCOLOR);
    return 0;
}

/*====================================
WORK WITH FILE SYSTEM RAHTER THAN DISK
======================================*/
int fill_disk(){
    fseek(fp, 0, SEEK_SET);
    fread(&g_vbs, sizeof(BootSector), 1, fp);
    if (g_vbs.trial_signature == TRIAL_SIGNATURE){
        g_disk.is_formatted = TRUE; 
        fseek(fp, g_vbs.FSinfo * g_vbs.bytes_per_sector, SEEK_SET);
        fread(&g_fsinfo, sizeof(FSinfo), 1, fp);

        g_disk.reserved_area = 0;
        g_disk.fat_area      = g_vbs.reserved_sectors * g_vbs.bytes_per_sector;
        g_disk.data_area     = g_disk.fat_area + (g_vbs.sectors_per_fat32 * 512 
                            * g_vbs.fat_count) + g_vbs.root_cluster_number * 512 * 4;
        g_disk.data_offset = 32;
        g_disk.fat32_offset = 4;
        g_disk.current_directory = g_disk.data_area;
        g_disk.read_block   = g_vbs.bytes_per_sector * g_vbs.sector_per_cluster;

        root_entry.directory_attributes = ATTR_VOLUME_ID | ATTR_DIRECTORY;
        root_entry.first_cluster_high = (g_vbs.root_cluster_number & 0xFFFF0000) >> 16;
        root_entry.first_cluster_lower = g_vbs.root_cluster_number & 0x0000FFFF;

        strncpy(g_disk.current_directory_name, "/", 11);
        fseek(fp, g_disk.current_directory, SEEK_SET);
        return 1;
    } 
    return 0;
}


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
    g_vbs.trial_signature     =  TRIAL_SIGNATURE;



    // FSINFO
    g_fsinfo.lead_signature = 0x41615252;
    memset(g_fsinfo.reserved1, '\0', 480);
    g_fsinfo.struct_signature = 0x61417272;
    g_fsinfo.free_clusters =  0xFFFFFFFF;
    g_fsinfo.next_free =  0xFFFFFFFF;
    memset(g_fsinfo.reserved2, '\0', 12);
    g_fsinfo.trial_signature = 0xAA550000;

    
    printf("%svolume Boot Sector size: %lu%s\n", 
    BLUE, sizeof(g_vbs), ENDCOLOR);

    if (wipe_disk(g_disk.disk_size) != 0){
        return -1;
    }

    g_disk.reserved_area = 0;
    g_disk.fat_area      = g_vbs.reserved_sectors * g_vbs.bytes_per_sector;
    g_disk.data_area     = g_disk.fat_area + (g_vbs.sectors_per_fat32 * 512 
                            * g_vbs.fat_count) + g_vbs.root_cluster_number * 512 * 4;
    g_disk.data_offset = 32;
    g_disk.fat32_offset = 4;
    g_disk.current_directory = g_disk.data_area;
    g_disk.read_block   = g_vbs.bytes_per_sector * g_vbs.sector_per_cluster;


    fseek(fp, g_disk.reserved_area, SEEK_SET);
    // RESERVED AREA
    fwrite(&g_vbs, sizeof(g_vbs), 1, fp);
    //FSinfo
    fseek(fp, (g_vbs.FSinfo * g_vbs.bytes_per_sector), SEEK_SET);
    fwrite(&g_fsinfo, sizeof(g_fsinfo), 1, fp);
    //BACKUP
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

    root_entry.directory_attributes = ATTR_VOLUME_ID | ATTR_DIRECTORY;
    root_entry.first_cluster_high = (g_vbs.root_cluster_number & 0xFFFF0000) >> 16;
    root_entry.first_cluster_lower = g_vbs.root_cluster_number & 0x0000FFFF;

    strncpy(g_disk.current_directory_name, "/", 11);
    g_disk.is_formatted = TRUE;
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
            
            printf("%snew allocated cluster number: %d%s\n",
            BLUE, j, ENDCOLOR);

            fseek(fp, g_disk.current_directory, SEEK_SET);
            return j;
        }
    }
    return -1;
}


//All it does is telling if file exist. And if so returns address to directory entry
int pathfinder(char *filepath, DirectoryEntry *dir_entry){
    
    char *token;
    uint32_t working_directory;

    if (filepath[0] == '/'){
        working_directory = g_disk.data_area;
    } else {
        working_directory = g_disk.current_directory;
    }

    fseek(fp, working_directory, SEEK_SET);
    token = strtok(filepath, "/");    
    
    if (token == NULL){
        memcpy(dir_entry, &root_entry, sizeof(DirectoryEntry));
        return g_disk.data_area;
    }

    while(token != NULL){

        start:
        for (int i = 0; i < g_disk.read_block; i += g_disk.data_offset){
            fread(dir_entry, sizeof(DirectoryEntry), 1, fp);
            if (strncmp(token, dir_entry->directory_name, 11) == 0){
                token = strtok(NULL, "/");
                if (token == NULL){
                    return (dir_entry->first_cluster_high << 16 | dir_entry->first_cluster_lower) * 
                    g_disk.read_block + g_disk.data_area - (2 * g_disk.read_block);
                }
                
                if (dir_entry->directory_attributes & ATTR_DIRECTORY){
                    working_directory = (dir_entry->first_cluster_high << 16 | dir_entry->first_cluster_lower) * 
                    g_disk.read_block + g_disk.data_area - (2 * g_disk.read_block);
                    fseek(fp, working_directory, SEEK_SET);
                    goto start;
                }
                fseek(fp, g_disk.current_directory, SEEK_SET);
                return -1;
            }
        }
        token = strtok(NULL, "/");        
    }
    fseek(fp, g_disk.current_directory, SEEK_SET);
    return -1;
}


int ls(char *pathname){
    
    DirectoryEntry dir_entry;
    uint32_t working_directory;

    char *color = GREEN; 
    char dir_mark = 'F';

    if (pathname != NULL){
        working_directory = pathfinder(pathname, &dir_entry);
        if (working_directory == -1){
            printf("%s%s%s\n", 
            RED, "Err...wrong path to dir", ENDCOLOR);
            return -1;
        }
        if ((dir_entry.directory_attributes & ATTR_DIRECTORY) == 0){
            printf("%s%s%s\n", 
            RED, "Err...file is not a directory", ENDCOLOR);
            return -1;
        }
    } else {
        working_directory = g_disk.current_directory;
    }

    fseek(fp, working_directory, SEEK_SET);

    while(1){
        for (int i = 0; i < g_disk.read_block; i += g_disk.data_offset){
            fread(&dir_entry, g_disk.data_offset, 1, fp);
            if ((dir_entry.creation_date | 0x00000000) == 0){
                return 0;
            }
            if (dir_entry.directory_attributes & ATTR_HIDDEN){
                continue;
            }

            if (dir_entry.directory_attributes & ATTR_DIRECTORY){
                color = MAGENTA;
                dir_mark = 'D';
            } else {
                color = GREEN;
                dir_mark = 'F';
            }
            printf("file: %s%11s\t%c%s\n",
            color, dir_entry.directory_name, dir_mark, ENDCOLOR);
        }
        break;
    }
    return 0;
}


int cd(char *pathname){
    
    DirectoryEntry dir_entry;

    uint32_t endfile;
    endfile = pathfinder(pathname, &dir_entry);
    

    if (endfile == -1){
        printf("%s%s%s\n",
        RED, "file does not exist", ENDCOLOR);
        return -1;
    }
    if ((dir_entry.directory_attributes & ATTR_DIRECTORY) == 0){
        printf("%sspecified file is not a directory bro...FILE: %s, %d%s\n",
        RED, dir_entry.directory_name, dir_entry.first_cluster_lower, ENDCOLOR);
        return -1;
    }

    g_disk.current_directory = endfile;
    
    if (endfile == g_disk.data_area){
        strncpy(g_disk.current_directory_name, "/", 11);
    } else {
        strncpy(g_disk.current_directory_name, dir_entry.directory_name, 11);
    }
    return 0;

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
        printf("%s%s%s\n",
        RED, "Error extracting time stuff", ENDCOLOR);
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
    if (isDir != 0){
        attributes |= ATTR_DIRECTORY;
    }

    dir_entry->directory_attributes = attributes;

    return;
}


int touch(char *pathname){
    
    DirectoryEntry dir_entry;
    uint32_t working_directory;
    char *cp_pathname = calloc(strlen(pathname), 1);
    char *file2create = calloc(strlen(pathname), 1);
    int walker;

    for (walker = strlen(pathname) - 1; walker >= 0; walker--){
        if (pathname[walker] == '/'){
            slice(pathname, cp_pathname, 0, walker);
            slice(pathname, file2create, walker + 1, strlen(pathname));
            break;
        }
    }

    if (walker == -1){
        cp_pathname = NULL;
        file2create = pathname;
    }


    if (cp_pathname != NULL){
        working_directory = pathfinder(cp_pathname, &dir_entry);
        if (working_directory == -1){
            printf("%s%s%s\n", 
            RED, "Err...wrong path to dir", ENDCOLOR);
            return -1;
        }
    } else {
        working_directory = g_disk.current_directory;
    }

    fseek(fp, working_directory, SEEK_SET);
    
    while(1){
        for (int i = 0; i < g_disk.read_block; i += g_disk.data_offset){
            // read each entry to find free space to plase new direcotry entry strcuture
            fread(&dir_entry, g_disk.data_offset, 1, fp);
            if (dir_entry.creation_time == 0){
                // fill dir_entry struct with meta information
                create_file(file2create, 0, &dir_entry);
                fseek(fp, working_directory + i, SEEK_SET);
                //write new dir entry inside directory
                fwrite(&dir_entry, sizeof(dir_entry), 1, fp);
                break;
            }
        }
        break;
    }
    return 0;
}


int my_mkdir(char *pathname){

    DirectoryEntry dir_entry;
    uint32_t working_directory;
    char *cp_pathname = calloc(strlen(pathname), 1);
    char *file2create = calloc(strlen(pathname), 1);
    int walker;

    for (walker = strlen(pathname) - 1; walker >= 0; walker--){
        if (pathname[walker] == '/'){
            slice(pathname, cp_pathname, 0, walker);
            slice(pathname, file2create, walker + 1, strlen(pathname));
            break;
        }
    }

    if (walker == -1){
        cp_pathname = NULL;
        file2create = pathname;
    }


    if (cp_pathname != NULL){
        working_directory = pathfinder(cp_pathname, &dir_entry);
        if (working_directory == -1){
            printf("%s%s%s\n", 
            RED, "Err...wrong path to dir", ENDCOLOR);
            return -1;
        }
    } else {
        working_directory = g_disk.current_directory;
    }

    fseek(fp, working_directory, SEEK_SET);

    while(1){
        for (int i = 0; i < g_disk.read_block; i += g_disk.data_offset){

            // read each entry to find free space to plase new direcotry entry strcuture
            fread(&dir_entry, g_disk.data_offset, 1, fp);
            if (dir_entry.creation_time == 0){
                // fill dir_entry struct with meta information
                create_file(file2create, 1, &dir_entry);
                fseek(fp, working_directory + i, SEEK_SET);
                //write new dir entry inside directory
                fwrite(&dir_entry, sizeof(dir_entry), 1, fp);
                break;
            }
        }
        break;
    } 
    return 1;
}

char **parse_tokens(char *line, int *argc){
    char **tokens;
    char *token;
    int len;

    tokens = malloc(MAX_TOKENS * sizeof(char *));
    token = strtok(line, " \n");
    int i = 0;

    while (token != NULL){
        len = strlen(token);
        tokens[i] = calloc(len * sizeof(char), 1);
        strncpy(tokens[i], token, len);
        token = strtok(NULL, " \n");
        i++;
    }

    *argc = i; 
    return tokens;
}


int command_launcher(char **tokens, int argc){
    
    /*
    commands with their number of arguments

    ls      - 1
    cd      - 1
    mkdir   - 1
    touch   - 1
    format  - 0
    */

    char* argv = NULL;

    if (argc > 2){
        printf("%s%s%s\n",
        RED, "Too many arguments...", ENDCOLOR);
        return -1;
    }

    if (argc == 2){
        argv = tokens[1];
    }

    if (strncmp(tokens[0], "ls", sizeof(tokens[0])) == 0){
        if (g_disk.is_formatted == TRUE){
            ls(argv);
        } else {
            printf("%s%s%s\n", 
            RED, "Err...Disk is not formatted. Please write format command", ENDCOLOR);
        }
    
    } else if (strncmp(tokens[0], "cd", sizeof(tokens[0])) == 0){
        if (g_disk.is_formatted == TRUE){
            if (argc != 2){
                printf("%s%s%s\n", 
                RED, "Err...cd <file name> usage", ENDCOLOR);
                return -1;
            }
            cd(argv);
        } else {
            printf("%s%s%s\n", 
            RED, "Err...Disk is not formatted. Please write format command", ENDCOLOR);
        }
    
    } else if (strncmp(tokens[0], "touch", sizeof(tokens[0])) == 0){
        if (g_disk.is_formatted == TRUE){
            if (argc != 2){
                printf("%s%s%s\n", 
                RED, "Err...touch <file name> usage", ENDCOLOR);
                return -1;
            }
            touch(argv);
        } else {
            printf("%s%s%s\n", 
            RED, "Err...Disk is not formatted. Please write format command", ENDCOLOR);
        }
    } else if (strncmp(tokens[0], "mkdir", sizeof(tokens[0])) == 0){
        if (g_disk.is_formatted == TRUE){
            if (argc != 2){
                printf("%s%s%s\n", 
                RED, "Err...mkdir <file name> usage", ENDCOLOR);
                return -1;
            }
            my_mkdir(argv);
        } else {
            printf("%s%s%s\n", 
            RED, "Err...Disk is not formatted. Please write format command", ENDCOLOR);
         }
    } else if (strncmp(tokens[0], "format", sizeof(tokens[0])) == 0){
        if (argc != 1){
            printf("%s%s%s\n",
            RED, "Too many arguments...", ENDCOLOR);
            return -1;
        }
        format_disk();
    } else {
        printf("%s%s%s\n",
        RED, "Err...Invalid command", ENDCOLOR);
        return -1;
    }
    return 0;
}


void loop(){

    char *prompt = ">";
    char line[MAX_COMMAND_LEN];
    char **tokens;
    int argc;

    while(1){
        printf("%s %s ", g_disk.current_directory_name, prompt);
        fgets(line, sizeof(line), stdin);
        tokens = parse_tokens(line, &argc);
        command_launcher(tokens, argc);
        fseek(fp, g_disk.current_directory, SEEK_SET);
        free(tokens);
    } 
}


int main(int argc, char *argv[])
{
    if (argc != 2){
        printf("%s%s%s\n",
        RED, "wrong usage. <command> <path to disk file>", ENDCOLOR);
        return -1;
    }
    
    char *pathname = argv[1];
    
    g_disk.is_formatted = FALSE;
    if (open_disk(pathname) == 0){
        loop(fp);
    }
    return 0;
}