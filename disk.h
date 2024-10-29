#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t disk_size;      // size of the file.
    uint8_t *absolute_path;  // stores file's path
    uint16_t reserved_area; // 0 frim which sector does reserved areas begin. Should always set to 0
    uint16_t fat_area;      // should set to bytes from which fat area begins
    uint16_t data_area;     // should set to bytes from which data area begins + root cluster offset
    uint16_t  entry_offset;  // for fat32 32. It simplifies working with fat32 and directory entries
} Disk;


int create_disk(char *pathname);
int open_disk(char *pathname, Disk *disk);
int wipe_disk(char *pathanme, size_t disk_size);