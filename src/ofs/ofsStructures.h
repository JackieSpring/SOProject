
#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "devicehndl.h"
#include "generic.h"

#define OFS_MAGIC 0x4c494f // OIL Little Endian
#define OFS_MAGIC_SIZE 3
#define OFS_VERSION 0x1
#define OFS_RESERVED_CLUSTER_COUNT 3
#define OFS_FILENAME_SAMPLE_SIZE 19
#define OFS_FILENAME_MAX_SAMPLES 1
#define OFS_FIRST_DATA_CLUSTER 2

typedef uint32_t OFSPtr_t;
typedef uint8_t OFSClsSize_t;
typedef uint64_t OFSSec_t;
typedef uint32_t OFSSecSize_t;
typedef  uint8_t OFSFlags_t;
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



typedef struct __attribute__((__packed__)) OFSDentryStruct {
    uint64_t        file_size;
    OFSFileName_t   file_name;
    OFSFlags_t      file_flags;
    OFSPtr_t        file_first_cls;
} OFSDentry_t ;



int ofsFormatDevice( DEVICE * dev );
bool ofsIsDeviceFormatted( DEVICE * dev );



