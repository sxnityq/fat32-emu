#include <stdint.h>
#include <stddef.h>
#include <stdio.h>


#define MIN_DISKSIZE 1048576 // 1M =  1024*1024 2*20
#define DEFAULT_DISKSIZE 20971520 //20M = 1024 * 1024 * 20. Used when pathname for disk does not exist 
#define MAX_DISKSIZE 268435456 // 256M 2**28

typedef struct {
    uint32_t disk_size;      // size of the disk file.
    uint8_t *absolute_path;  // stores file's path
    uint16_t reserved_area; // 0 frim which sector does reserved areas begin. Should always set to 0
    uint16_t fat_area;      // should set to bytes from which fat area begins
    uint16_t data_area;     // should set to bytes from which data area begins + root cluster offset
    uint16_t data_offset;  // for data area 32. It simplifies working with directory entries
    uint8_t  fat32_offset; // for fat32 entry aka 4 byte
    uint32_t current_directory; // number of bytes from beginning of the disk file that points to current directory 
    uint8_t current_directory_name[11]; // name for current directory to show in cd
    uint16_t read_block;        // sectors per cluster * bytes in sector
    uint8_t is_formatted;       // True if Volume Boot signature is set
} Disk;


int create_disk(char *pathname);
int open_disk(char *pathname);
int wipe_disk(size_t disk_size);
int fill_disk();