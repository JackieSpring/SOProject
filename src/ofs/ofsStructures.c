
#include "ofsStructures.h"


#define OFS_MAGIC 0x4c494f // OIL Big Endian
#define OFS_VERSION 0x1

typedef OFSPtr_t uint32_t;


/*
    n >= 4 / ( p * s^2 )    // dimensione del cluster per un consumo della FAT di p% settori

    e = 1 / ( s * n )       // indice di frammentazione dei settori

    al crescere della dimensione di un cluster si riduce la dimensione della FAT, 
    ma aumenta la frammentazione 
*/


struct __attribute__((__packed__)) OFSBootStruct {
    uint32_t magic      : 24;
    uint8_t  version;
    uint32_t sec_size   : 24;
    uint8_t  cls_size;
    uint64_t sec_cnt;
    OFSPtr_t root_dir_ptr;
    uint32_t free_cls_cnt;
    OFSPtr_t free_ptr;
};

void __attribute__((constructor)) a(){
    printf( "size: %d\n", sizeof(OFSBoot) );
}

struct OFSDentryStruct;


int ofsFormatDevice( DEVICE * dev ) {
    OFSBoot boot;

    boot.magic = OFS_MAGIC;
    boot.version = OFS_VERSION;
    boot.sec_size = dev->dstat.st_blksize;
    boot.cls_size = 0; // DA CALCOLARE
    boot.sec_cnt = dev->dstat.st_blocks;
    boot.root_dir_ptr = 0; // DA CALCOLARE
    boot.free_cls_cnt = 0; // DA CALCOLARE
    boot.free_ptr = 0; // DA CALCOLARE
}


