
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "../utils/devicehndl.h"
#include "../utils/generic.h"

#define OFS_MAGIC 0x4c494f // OIL Little Endian
#define OFS_MAGIC_SIZE 3
#define OFS_VERSION 0x1

#define OFS_RESERVED_CLUSTER_COUNT 3
#define OFS_FILENAME_SAMPLE_SIZE 18
#define OFS_FILENAME_MAX_SAMPLES 1
#define OFS_FIRST_DATA_CLUSTER 2


#define OFS_FLAG_FREE       0
#define OFS_FLAG_DIR        1 << 0
#define OFS_FLAG_FILE       1 << 1
#define OFS_FLAG_RDONLY     1 << 2
#define OFS_FLAG_INVALID    1 << 3
#define OFS_FLAG_HIDDEN     1 << 4



typedef uint32_t OFSPtr_t;
typedef uint8_t OFSClsSize_t;
typedef uint64_t OFSSec_t;
typedef uint32_t OFSSecSize_t;
typedef uint8_t OFSFlags_t;
typedef uint8_t OFSFileNameSize_t;
typedef char OFSFileName_t [OFS_FILENAME_SAMPLE_SIZE] ;


enum OFSPtrCode {
    OFS_MAX_CLUSTER = -3,
    OFS_RESERVED_CLUSTER = -2,
    OFS_INVALID_CLUSTER = -1,
    OFS_LAST_CLUSTER = 0,
};


typedef struct __attribute__((__packed__)) OFSBootStruct {
    uint32_t magic      : 24;
    uint8_t  version;

    OFSSecSize_t sec_size   : 24;
    OFSClsSize_t  cls_size;
    OFSSec_t sec_cnt;
    OFSPtr_t cls_cnt;

    OFSPtr_t root_dir_ptr;  // Root directory 
    OFSPtr_t free_cls_cnt;
    OFSPtr_t free_ptr;      // First free cluster

    OFSSec_t fat_sec;       // FAT start sector
    OFSSec_t first_sec;     // First data sector after FAT
} OFSBoot_t;

//  01234567 89abcdef
// | fsize | nsize | fname  | flag | cls |
// 0       7       9     25 26  27 28  32  
typedef struct __attribute__((__packed__)) OFSDentryStruct {
    uint64_t        file_size;          // 8
    OFSFileNameSize_t file_name_size;   // 1
    OFSFileName_t   file_name;          // 18
    OFSFlags_t      file_flags;         // 1
    OFSPtr_t        file_first_cls;     // 4
} OFSDentry_t ;                         // 32



int ofsFormatDevice( DEVICE * dev );
bool ofsIsDeviceFormatted( DEVICE * dev );

extern inline OFSSec_t ofsClusterToSector( OFSBoot_t * ofsb, OFSPtr_t cp ) ;
extern inline OFSPtr_t ofsSectorToCluster( OFSBoot_t * ofsb, OFSSec_t sp );
extern inline off_t ofsSectorToByte( OFSBoot_t * ofsb, OFSSec_t sp );

