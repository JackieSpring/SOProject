
#pragma once

#include <stdlib.h>
#include <sys/mman.h>

#include "generic.h"
#include "devicehndl.h"
#include "ofsStructures.h"



typedef struct OFSStruct_t {
    DEVICE    * dev;

    OFSBoot_t * boot;
    OFSPtr_t  * fat;
    uint64_t    fat_size;   // Bytes
} OFS_t;

/**
 *  Apre il file system e ritorna un handler
 *
 *  @param  dev     Device su cui è stato formattato il 
 *                  file system
 *
 *  @return         Handler per manipolare il file system
*/
OFS_t * ofsOpen( DEVICE * dev );

/**
 *  Chiude l'handler del file system aperto
*/
void ofsClose( OFS_t * ofs );

