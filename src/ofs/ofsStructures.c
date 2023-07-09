
#include "ofsStructures.h"



/*
    n >= 4 / ( p * s^2 )    // dimensione del cluster per un consumo della FAT di p% settori
                            // al numeratore va il numero di byte che compongono un indirizzo

    e = 1 / n               // indice di frammentazione dei settori

    s = 4 e / p             // legame settori-consumo-frammentazione

    al crescere della dimensione di un cluster si riduce la dimensione della FAT, 
    ma aumenta la frammentazione 
*/




void __attribute__((constructor)) a(){
    printf( "ofsStructures size: %d\n", sizeof(OFSBoot_t) );
}



static inline OFSClsSize_t ofsCalculateClusterSize( OFSBoot_t * ofsb ) {
    return 2;   // TODO migliorare il calcolo
}

static inline OFSPtr_t ofsCalculateClusterCount( OFSBoot_t * ofsb ) {
    OFSPtr_t cls_cnt = (( ofsb->sec_cnt ) / ofsb->cls_size ) - 1 ; // alloco un cluster per boot
    cls_cnt -= 2; // alloco due cluster per la fat table (MIGLIORARE)
    if ( cls_cnt > (OFSPtr_t)OFS_MAX_CLUSTER )
        cls_cnt = (OFSPtr_t)OFS_MAX_CLUSTER;
    
    return (uint64_t) cls_cnt ;
}

static inline OFSSec_t ofsCalculateFatStart ( OFSBoot_t * ofsb ) {
    return ofsb->cls_size;  // riserva un cluster per la boot structure
}

static inline OFSSec_t ofsCalculateFirstSec( OFSBoot_t * ofsb ) {

    register uint64_t bytes = ofsb->cls_cnt * sizeof( OFSPtr_t ) ;
    register uint64_t sec_bytes = ofsb->sec_size * ofsb->cls_size ;

    if ( bytes % sec_bytes )
        bytes += sec_bytes - (bytes % sec_bytes);
    
    return ((bytes / sec_bytes) * ofsb->cls_size) + ofsb->fat_sec ;
}


static inline OFSSec_t ofsClusterToSector( OFSBoot_t * ofsb, OFSPtr_t cp ) {
    return ofsb->first_sec + ( cp * ofsb->sec_size * ofsb->cls_size );
}

static inline OFSPtr_t ofsSectorToCluster( OFSBoot_t * ofsb, OFSSec_t sp ){
    return ( sp - ofsb->first_sec ) / ( ofsb->sec_size * ofsb->cls_size ) ;
}





int ofsFormatDevice( DEVICE * dev ) {
    OFSBoot_t boot;
    OFSDentry_t rootd;

    boot.magic = OFS_MAGIC;
    boot.version = OFS_VERSION;
    boot.sec_size = dev->dstat.st_blksize;
    boot.sec_cnt = dev->dstat.st_blocks;
    boot.root_dir_ptr = 0;  // DA CALCOLARE
    boot.free_cls_cnt = 0;  // DA CALCOLARE
    boot.free_ptr = 0;      // DA CALCOLARE

    //DEBUG
    boot.sec_size = 512;
    boot.sec_cnt = 50;
    //DEBUG

    boot.cls_size = ofsCalculateClusterSize( &boot );
    boot.cls_cnt = ofsCalculateClusterCount( &boot );

    boot.fat_sec = ofsCalculateFatStart( &boot );
    boot.first_sec = ofsCalculateFirstSec( &boot );

    printf( "cls_cnt: %d\n", boot.cls_cnt );
    printf( "cls_size: %d\n", boot.cls_size );
    printf( "fat: %d\n", boot.fat_sec );
    printf( "first: %d\n", boot.first_sec );

    // Formatta la fat scrivendola un cluster per volta
    // le prime due entry della fat sono cluster riservati,
    // il file system incomincia dal cluster [2]
    // tutte le entry non corrispondenti a cluster reali
    // (ovvero quelle oltre la tabella) sono marchiate come
    // INVALID

    OFSPtr_t fatPages = ( boot.first_sec - boot.fat_sec ) / boot.cls_size ;
    uint64_t fatPageSize = boot.cls_size * boot.sec_size ;
    uint64_t fatPageEntries = fatPageSize / sizeof( OFSPtr_t ) ;

    OFSPtr_t * fat_page = ( OFSPtr_t * ) malloc( fatPageSize ) ;
    OFSPtr_t counter = 0;

    for ( OFSPtr_t page = 0 ; page < fatPages; page++ ) {

        for ( uint64_t i = 0; i < fatPageEntries; i++ ){
            fat_page[i] = counter;
            counter++;
        }

        if ( page == (fatPages - 1) ) {
            register uint64_t ovf = counter - boot.cls_cnt ;
            memset( &fat_page[ fatPageEntries - ovf ], (OFSPtr_t) OFS_INVALID_CLUSTER, ovf * sizeof(OFSPtr_t) );
            fat_page[ fatPageEntries - ovf - 1 ] = OFS_LAST_CLUSTER;
        }

        if ( page == 0 ) {
            fat_page[0] = (OFSPtr_t) OFS_RESERVED_CLUSTER;
            fat_page[1] = (OFSPtr_t) OFS_RESERVED_CLUSTER;
        }

        writeDev( dev, fat_page, fatPageSize, (boot.fat_sec * boot.sec_size) + (fatPageSize * page ) );
    }

    // ROOT directory

    // Boot 


    return 0;

cleanup:

    if ( fat_page )
        free(fat_page);

    return -1;
    
}