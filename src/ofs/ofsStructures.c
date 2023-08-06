
#include "ofsStructures.h"

/*

*/




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


extern inline OFSSec_t ofsClusterToSector( OFSBoot_t * ofsb, OFSPtr_t cp ) {
    return ofsb->first_sec + ( cp * ofsb->cls_size );
}

extern inline OFSPtr_t ofsSectorToCluster( OFSBoot_t * ofsb, OFSSec_t sp ){
    return ( sp - ofsb->first_sec ) / ( ofsb->cls_size ) ;
}

extern inline off_t ofsSectorToByte( OFSBoot_t * ofsb, OFSSec_t sp ) {
    return sp * ofsb->sec_size ;
}




int ofsFormatDevice( DEVICE * dev ) {

    puts("DO FORMAT");

    OFSBoot_t boot;
    OFSDentry_t rootd;
    int ret = 0;

    boot.magic = OFS_MAGIC;
    boot.version = OFS_VERSION;
    boot.sec_size = dev->blksize;
    boot.sec_cnt = dev->blkcnt;
    boot.root_dir_ptr = OFS_INVALID_CLUSTER;  // DA CALCOLARE
    boot.free_cls_cnt = 0;  // DA CALCOLARE
    boot.free_ptr = 0;      // DA CALCOLARE


    boot.cls_size = ofsCalculateClusterSize( &boot );
    boot.cls_cnt = ofsCalculateClusterCount( &boot );

    boot.fat_sec = ofsCalculateFatStart( &boot );
    boot.first_sec = ofsCalculateFirstSec( &boot );


    // Formatta la fat scrivendola un cluster per volta
    // le prime due entry della fat sono cluster riservati,
    // il file system incomincia dal cluster [2]
    // tutte le entry non corrispondenti a cluster reali
    // (ovvero quelle oltre la tabella) sono marchiate come
    // INVALID
    // Inizializza root directory (vuota) e linked clist di
    // cluster liberi, inoltre inizializza i relativi campi
    // nel boot sector 

    OFSPtr_t fatPages = ( boot.first_sec - boot.fat_sec ) / boot.cls_size ;
    uint64_t fatPageSize = boot.cls_size * boot.sec_size ;
    uint64_t fatPageEntries = fatPageSize / sizeof( OFSPtr_t ) ;

    OFSPtr_t * fat_page = ( OFSPtr_t * ) malloc( fatPageSize ) ;
    OFSPtr_t counter = 0;

    for ( OFSPtr_t page = 0 ; page < fatPages; page++ ) {

        for ( uint64_t i = 0; i < fatPageEntries; i++ ){
            fat_page[i] = counter + 1;
            counter++;
        }

        if ( page == (fatPages - 1) ) {
            register uint64_t ovf = counter - boot.cls_cnt ;
            memset( &fat_page[ fatPageEntries - ovf ], (OFSPtr_t) OFS_INVALID_CLUSTER, ovf * sizeof(OFSPtr_t) );
            fat_page[ fatPageEntries - ovf - 1 ] = OFS_LAST_CLUSTER;
        }

        // Inizializza cluster riservati, cluster liberi e root directory
        if ( page == 0 ) {
            for ( uint64_t i = 0; i < OFS_FIRST_DATA_CLUSTER ; i++ )
                fat_page[i] = (OFSPtr_t) OFS_RESERVED_CLUSTER;
            
            //fat_page[ OFS_FIRST_DATA_CLUSTER ] = OFS_LAST_CLUSTER;
            //boot.root_dir_ptr = OFS_FIRST_DATA_CLUSTER;

            boot.free_ptr = OFS_FIRST_DATA_CLUSTER;
            boot.free_cls_cnt = boot.cls_cnt - OFS_FIRST_DATA_CLUSTER;

        }

        ret = writeDev( dev, fat_page, fatPageSize, (boot.fat_sec * boot.sec_size) + (fatPageSize * page ) );
        if ( ret < 1 )
            goto cleanup;
    }


    // Boot 
    ret = writeDev( dev, &boot, sizeof( OFSBoot_t ), 0 );
    if ( ret < 1 )
        goto cleanup;


    return 0;

cleanup:

    if ( fat_page )
        free(fat_page);

    return -1;
    
}

bool ofsIsDeviceFormatted( DEVICE * dev ) {
    uint32_t magic = 0;

    puts("IS FORMATTED");

    if ( readDev( dev, &magic, 3, 0 ) < 3 )
        return false;

    if ( magic - OFS_MAGIC )
        return false;

    return true;
    
}