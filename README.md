# FAT32 emulator
❗ Test assignment for INANGO

## VERY IMPORTANT
1)  MANY FEATURES IN OG FAT32 ARE DROPPED FOR THE SAKE OF SIMPLICITY (SEE ... FOR DETAILS)
2)  MINIMUM SIZE OF DISK FILE IS SET TO 1M. IT IS NOT RECOMMENDED TO REDEFINE THIS VALUE
3)  DURING WRITING & TESTING I HAVE FOUND SOME SMALL BUGS. SO EVEN IF IT SEEMS FOR ME TO BE OKAY IT STILL MIGHT BE POSSIBLE FOR U TO CATCH SOME ISSUES. PLEASE CONTACT ME IF U DISCOVER SOMETHING UNUSUAL


## STRUCTURE

okay, here we go

```.
├── theory.txt
├── README.md
├── Makefile
├── fat32.h
├── fat32.c
├── disk.h
├── commands.h
└── collors.h
```
**theory.txt** - contains my notes during learning theory about fat32. Nothing special for U. I left it only to show U the way I was thinking.
**fat32.h** - stores all neccesseary structures like Volume Boot Sector, FSinfo, Directory Entry.
**disk.h** - Stores all assists tructures that simplifies the way I can operate on disk file. Also store some disk specific function declarations
**commands.h** - Stores commands declarations.
**collors.h** - Stores some shimer and shine shit
**fat32.c** - 650+ divine intellect lines of code

## VOLUME BOOT SECTOR, FSINFO & DIR ENTRY
Volume Boot Sector is usually 512 bytes entry and contains meta information about how to work with whole fat32 volume. Is located alwats at first sector on the partition
 
    boot_jump_instruction -> asm instruction to jump to boot code. NOT USED
    OEM_identifier -> mark who was in charge for formating
    bytes_per_sector -> how many bytes fits in 1 sector. 512 BY DEFAULT
    sector_per_cluster -> how many sectors fits in 1 cluser. 4 BY DEFAULT
    reserved_sectors -> how many sectors in reserved area. 32 BY DEFAULT
    fat_count -> number of FAT tables. 2 BY DEFAULT
    root_enrtry_count -> how many dir entries in root dir. NOT USED IN FAT32 
    total_sectors16 -> how many sectors in whole disk file. NOT USED
    media_descriptor -> type of media to used. DEFAULT 0xF8. DONT MAKE ANY SENSE
    sectors_per_fat16 -> sectors per FAT table. NOT USED IN FAT32    
    sectors_per_track -> dectors per track. DONT MAKE ANY SENSE
    number_of_heads -> heads of hdd. DONT MAKE ANY SENSE
    hidden_sectors -> number of sectors before VBR. DONT MAKE ANY SENSE
    total_sectors32 -> total sectors on disk file
    sectors_per_fat32 -> sectors per FAT table
    ext_flags -> Some legacy shit that I dont figured out. NOT USED
    file_system_version -> file system version. NOT USED
    root_cluster_number -> number of root cluster beggining after FAT area. DEFAULT 2
    
    FSinfo -> sector in reservsed aread from which fsinfo begins. DEFAULT 1
    backup_boot_sector -> sector in reservsed aread from which backup begins for VBR and FSinfo. DEFAULT 6
    
    reserved1 -> 12 reserved bytes
    drive_number -> some shit that separates floppy disk and solid
    reserved2 -> again reserved shit
    boot_signature -> indicates that following 3 fiels make sense. DONT MAKE SENSE 
    serial_number -> volume serial number. DONT MAKE SENSE
    volume_label -> volume lable. DEFAULT NONAME. DONT MAKE SENSE
    file_system_type -> FAT type (32/16/12). Btw it is not used even in OG FAT         
    boot_code -> 420 bytes of bootstrap code. DONT MAKE SENSE
    trial_signature -> signature for verifying that it is correct volume. DEFAULT 0xAA55

FSinfo contains information fow better perfomance. Is used to optimising process of finding free clusters. I DONT USE FSINFO
    
    lead_signature -> Indicate that it is indeed Fsinfo entry. DEFAULT 0x41615252.
    DONT MAKE SENSE
    reserved1 -> reserved shit 1
    struct_signature -> One more signature. DEFAULT 0x61417272. DONT MAKE SENSE
    free_clusters -> contains free clusters
    next_free -> hint for FAT driver where to search free clusters. Otherwise start from 2
    
    reserved2 -> reserved shit 2
    trial_signature -> signature for verifying that it is correct fsinfo entry. DEFAULT 0xAA5500

Directory entry contains information about file. 

    directory_name -> 11 bytes name
    directory_attributes -> some attributes
    directory_NTRes -> some shit that i dont figured out
    creation_time_tenth -> creation time of tenths seconds. NEVER USED
    creation_time -> time of creation. SHOULD NOT BE MODIFIED
    creation_date -> date of creation. SHOULD NOT BE MODIFIED
    last_access_date -> last access date
    first_cluster_high -> high 16 bits of cluster number
    write_time -> time of last write
    write_date -> date of last write
    first_cluster_lower -> lower 16 bits of cluster number
    file_size -> file size in bytes

ATTRIBUTES:
-    ATTR_READ_ONLY 0x01
-    ATTR_HIDDEN 0x02 (for files with trailing dots)
-    ATTR_SYSTEM 0x04
-    ATTR_VOLUME_ID 0x08 (only for root)
-    ATTR_DIRECTORY 0x10
-    ATTR_ARCHIVE 0x20
-    ATTR_LONG_NAME 

## Disk, root_entry
Disk is my own structure that helps me with sector math

    disk_size -> size of the disk file.
    absolute_path -> path at host to disk file
    reserved_area -> how far in bytes is reserved area from beggining of disk file. DEFAULT 0
    
    fat_area ->  how far in bytes is fat area from beggining of disk file
    data_area ->  how far in bytes is data area from beggining of disk file + root_cluster offset
    
    data_offset ->  Size of directory entry. DEFAULT 32. perhaps useless
    fat32_offset -> Size of Fat32 entry. DEFAULT 4
    current_directory -> umber of bytes from beginning of the disk file that points to current directory 
    current_directory_name -> name for current directory to show in cd
    read_block -> sectors per cluster * bytes in sector
    is_formatted -> True if Volume Boot signature is set

root_entry it simpy is a directory entry with ATTR_VOLUME_ID to be set. It is used for commands with slash (/) as argv to point to root

## USAGE
```sh
make fat32
./fat32 <path to disk file>
```
Inside the loop u are greeted by interactive mode.
Available commands are:
*   cd
*   ls
*   format
*   mkdir
*   touch

ABSOLUTE PATH IS AVAILABLE!!!!1!1. FEEL FREE TO USE. ONLY ".." IS NOT IMPLEMENTED. SORRY)
```
disk file: test size: 20971521
 > format
volume Boot Sector size: 512
disk file successfully wiped! Congrats
/ > ls
/ > mkdir HOLLOW_K             
new allocated cluster number: 3
/ > cd /HOLLOW_K
HOLLOW_K > touch dirtmouth
new allocated cluster number: 4
HOLLOW_K > touch /HOLLOW_K/greenpath
new allocated cluster number: 5
HOLLOW_K > cd /
/ > touch HOLLOW_K/cityOFtears
new allocated cluster number: 6
/ > ls /HOLLOW_K
file:   dirtmouth       F
file:   greenpath       F
file: cityOFtears       F
/ >
```

## LIMITATIONS

1) ONLY SUPPORTED 1 CLUSTER CHAIN. It means that u can ls only 4096 / data_offset files. Perhaps will fix later.
2) I DONT USE FSINFO! Perhaps also later...
3) AND I DONT IMPLEMENTED LONG FILE NAMES!

![tux go brrr](https://github.com/sxnityq/fat32-emu/blob/main/assets/tux.gif)