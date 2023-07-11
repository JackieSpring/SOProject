
#include "ofsModel.h"



OFS_t * ofsOpen( DEVICE * dev ) {
    if ( dev == NULL )
        goto cleanup;

    OFS_t * ofs = NULL;

    ofs = (OFS_t *) calloc( 1, sizeof(OFS_t) );
    if ( ofs == NULL )
        goto cleanup;

    ofs->dev = dev;
    ofs->boot = MAP_FAILED;
    ofs->fat = MAP_FAILED;
    ofs->fat_size = 0;

    // Mappa in memoria l'intero boot sector del file system
    // in modo da potervi facilemnte accedere come struttura

    ofs->boot = mmap( NULL, sizeof(OFSBoot_t), PROT_READ | PROT_WRITE, MAP_SHARED, dev->dd, 0 );
    if ( ofs->boot == MAP_FAILED )
        goto cleanup;

    // Mappa in memoria la tabella FAT, poiché i settori mappati
    // essere allineati con le pagine di memoria e le dimensioni
    // di questi due potrebbero non coincidere sempre, prima
    // calcolo l'offset dall'allineamento "piu vicino", poi 
    // alloco la sezione richiesta e infine ignoro i dati in eccesso

    OFSSec_t fatStart = ofs->boot->fat_sec;
    OFSSec_t fatEnd = ofs->boot->first_sec;
    OFSSecSize_t secSize = ofs->boot->sec_size;
    long pageSize = sysconf( _SC_PAGESIZE );
    off_t ovf;
    uint8_t * fptr;

    ofs->fat_size = (fatEnd - fatStart) * secSize;
    ovf = (fatStart * secSize) % pageSize;

    fptr = mmap( NULL, ofs->fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev->dd, (fatStart * secSize) - ovf );
    if ( fptr == MAP_FAILED )
        goto cleanup;

    ofs->fat = fptr + ovf;

    
    return ofs;

cleanup:

    if ( ofs )
        free(ofs);

    if ( ofs->boot != MAP_FAILED )
        munmap( ofs->boot, sizeof(OFSBoot_t) );

    if ( ofs->fat != MAP_FAILED )
        munmap( ofs->fat, ofs->fat_size );

    return NULL;
}

void ofsClose( OFS_t * ofs ){

    munmap( ofs->fat, ofs->fat_size );
    munmap( ofs->boot, sizeof(OFSBoot_t) );
    free(ofs);

    return;
}

