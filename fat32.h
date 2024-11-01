#include <stdint.h>
#include <stddef.h>


#define BAD_BLOCK       0xFFFFFFF7
#define FREE_BLOCK      0x00000000
#define RESERVED_BLOCK  0xFFFFFFF6
#define EOC_BLOCK       0xFFFFFFF8

#define CleanShutBitMask 0x80000000
#define HardErrBitMask   0x40000000

/* file attribute in combination of following flags. 
Upper 2 bits are reserved and must be zero.
*/
#define ATTR_READ_ONLY          0x01 // (Read-only) Do not allow to modify file 
#define ATTR_HIDDEN             0x02 // (Hidden) Not show files while using ls
#define ATTR_SYSTEM             0x04 // (System) for some system files idk
#define ATTR_VOLUME_ID          0x08 // (Volume label) Only for root directory
#define ATTR_DIRECTORY          0x10 // (Directory) For mark that file is directory
#define ATTR_ARCHIVE            0x20 // (Archive) for backup utilities
#define ATTR_LONG_FILE_NAME     0x0F // (LFN entry) For long file name support

#define OEM "INANGO\0"


// this struct gives everything u have to know about physical layout of my FAT32 emu
typedef struct 
{
    uint8_t  boot_jump_instruction[3]; // i dont use it. Left for backward compatability
    uint8_t  OEM_identifier[8];      // INANGO\0 will be used
//  #######START BPB (Bios Parametr Block)
    uint16_t bytes_per_sector;        // 512 how many bytes in 1 sector (512 by default and idk if it even make sense to change)
    uint8_t  sector_per_cluster;      // 2 how many sectors are in 1 logical cluster (for our purposes and simplicity 2 will be enough) 
    uint16_t reserved_sectors;       // 32 how far in sectors is first FAT table located (Number of reserved sectors in the Reserved region of the volume starting at the first sector of the volume))
    uint8_t  fat_count;               // 2 number of FAT tables
    uint16_t root_enrtry_count;       // in FAT32 set to 0. In FAT12/16 it contains the count of 32-byte directory entries in the root directory
    uint16_t total_sectors16;        // in FAT32 set to 0. In FAT12/16 it stores total number of sectors on the volume
    uint8_t  media_descriptor;        // 0xF8 is the standard value for “fixed” (non-removable) media. 0xF0 for removable (usb drive).
    uint16_t sectors_per_fat16;        // In FAT 32 set to 0; in FAT12/16 set numbers of sectors occupied by FAT Table
    uint16_t sectors_per_track;       // Only make sense for disk geomerties drives
    uint16_t number_of_heads;         // Only make sense for disk geomerties drives;
    uint32_t hidden_sectors;         //  The number of sectors on the volume before the volume boot sector
    uint32_t total_sectors32;        // Total number of sectors on the volume
    uint32_t sectors_per_fat32;       // set number of sectors occupied by one FAT table
    uint16_t ext_flags;              // Some legacy shit that I dont figured out
    uint16_t file_system_version;     // 00 00.
    uint32_t root_cluster_number;     // 2 cluster number of the first cluster of the root directory
    uint16_t FSinfo;                // 1 Sector number of FSINFO structure in the reserved area
    uint16_t backup_boot_sector;       // 6 sector in reserved area where Boot Sector copy will be stored
    uint8_t  reserved1[12];           // reserved shit 
//  ########END BPB  
    uint8_t  drive_number;            // 00 Set to 0x80 if active volume
    uint8_t  reserved2;              // again reserved
    uint8_t  boot_signature;          // 0x29 
    uint32_t serial_number;           // volume serial number. Ususally created by combining current date and time
    uint8_t  volume_label[11];         // NO NAME some labeling perhaps. 
    uint8_t  file_system_type[8];        // FAT32 for FAT32          
    // offeset 82
    uint8_t boot_code[420];           // Bootstrap program
    uint16_t trial_signature;         //0xAA55. A boot signature indicating that this is a valid volume boot sector.
} __attribute__((packed)) BootSector;


typedef struct
{
    uint32_t lead_signature; // 0x41615252. Is used to validate that this sector is indeed FSinfo
    uint8_t reserved1[480]; // reservde for future use. Should be filled with 0
    uint32_t struct_signature; // 0x61417272 One more signature
    uint32_t free_clusters; // Contains the last free clusters cound. 0xFFFFFFFF - means free is unknown
    uint32_t next_free;      // hint for FAT driver where to search free clusters. Typically this value isset to the last cluster number that the driver allocated. If the value is 0xFFFFFFFF, then there is no hint and the driver should start looking at cluster 2
    uint8_t reserved2[12];  // one more reserved shit
    uint32_t trial_signature; //Value 0xAA550000. This trail signature is used to validate that this is in fact an FSInfo sector
} FSinfo;


typedef struct
{
    uint8_t directory_name[11]; // Short Name
    uint8_t directory_attributes; 
    uint8_t directory_NTRes; //Optional flags that indicates case information of the SFN.
    uint8_t creation_time_tenth; //   Millisecond stamp at file creation time. This field actually contains a count of tenths of a second.
    uint16_t creation_time; //   Time file was created
    uint16_t creation_date; //   Date file was created
    uint16_t last_access_date; // Last acces date
    uint16_t first_cluster_high; //Uper part of cluster number
    uint16_t write_time;     //Time of last write
    uint16_t write_date;     // Date of last write
    uint16_t first_cluster_lower;     //Lower part of cluster number
    uint32_t file_size;          //file size in bytes
} __attribute__((packed)) DirectoryEntry;

typedef uint32_t Fat32entry;

typedef struct
{
    uint32_t mark;
} Fat32Entry;


int get_optimal_FAT32sectors(size_t disk_size);
