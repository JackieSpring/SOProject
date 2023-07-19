
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "../utils/generic.h"
#include "../utils/devicehndl.h"
#include "ofsStructures.h"
#include "ofsListType.h"



typedef struct OFSFileStruct_t {
    OFSFileName_t * name;
    OFSFileNameSize_t name_size;
    OFSPtrList_t  * cls_list;
    OFSPtr_t        parent_dir;
    OFSFlags_t      flags;
    uint64_t        size;
} OFSFile_t ;


typedef struct OFSClusterStruct_t {
    OFSPtr_t        cls_number;
    OFSPtr_t        next;
    uint8_t     *   data;
    uint64_t        size;
} OFSCluster_t;



typedef struct OFSStruct_t {
    DEVICE    * dev;

    OFSBoot_t * boot;
    OFSPtr_t  * fat;
    uint64_t    fat_size;   // Bytes
    uint64_t    cls_bytes;

    OFSFile_t * root_dir;
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

/**
 *  Ottiene un oggetto Cluster da un puntatore a cluster
 *  
 *  @param  ofs     Handle del file system
 *  @param  ptr     Puntatore (indice) al cluster
 *
 *  @return         Puntatore alla struttura cluster o NULL  
*/
OFSCluster_t * ofsGetCluster( OFS_t * ofs, OFSPtr_t ptr );

/**
 *  Dealloca la struttura Cluster ottenuta da ofsGetCluster
 *
 *  @param  cls     CLuster da deallocare
*/
void ofsFreeCluster( OFSCluster_t * cls );


/**
 *  Ottiene un file handle per la dentry specificata
 *  se il file dovesse avere OFS_FLAG_INVALID attiva
 *  allora l'handle non verrà generato
 *
 *  @param  dentry      Puntatore alla dentry che rappresenta
 *                      il file
 *
 *  @return             Ritorna un puntatore all'handle o NULL
*/
OFSFile_t * ofsOpenFile( OFS_t * ofs, OFSDentry_t * dentry );

/**
 *  Chiude l'handle del file e libera le sue risorse
 *  @param  file    handle del file
*/
void ofsCloseFile( OFSFile_t * file );


OFSDentry_t * ofsGetDentry( OFS_t * ofs, OFSFile_t * dir, const char * fname, size_t fnsize );
void ofsFreeDentry( OFSDentry_t * dent );

