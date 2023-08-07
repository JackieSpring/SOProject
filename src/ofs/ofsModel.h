
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "../utils/generic.h"
#include "../utils/devicehndl.h"
#include "../utils/numhashtable.h"
#include "../utils/array.h"
#include "ofsStructures.h"
#include "ofsListType.h"



#define OFS_DIR_NO_ENTRIES -1
#define OFS_FHMEM_LAST  -1
#define OFS_FHMEM_INIT_SIZE 0x10


//  OGGETTI FILE
//  rappresentano un file del file system,
//  contengono tutte le sue informazioni generali
//  e il numero di "riferimenti" ovvero il numero
//  di handles attualmente attivi su questo file
typedef struct OFSFileStruct_t {
    OFSFileName_t * name;
    OFSFileNameSize_t name_size;
    OFSPtrList_t  * cls_list;
    OFSFlags_t      flags;
    uint64_t        size;
    NumHTKey_t      fomem_key;
    size_t          refs;
} OFSFile_t ;

typedef struct OFSDirStruct_t {
    OFSFile_t   super;
    off_t    last_entry_index;
    off_t    entries;
} OFSDir_t;

//  FILE HANDLES
//  Rappresentano degli "handle" dei file
//  ovvero dei "riferimenti" aperti dai
//  processi
typedef struct OFSFileHandleStruct_t {
    int         open_flags;
    off_t       seek_ptr;
    off_t       fhmem_idx;
    NumHTKey_t  fomem_idx;
} OFSFileHandle_t;


//  CLUSTER OBJECT
//  Rappresenta un singolo cluster del file
//  system, contiene la sua dimenzione e 
//  un puntatore ai dati del cluster
//  mappati in memoria
typedef struct OFSClusterStruct_t {
    OFSPtr_t        cls_number;
    OFSPtr_t        next;
    uint8_t     *   data;
    uint64_t        size;
} OFSCluster_t;


//  FILE SYSTEM OBJECT
//  Rappresenta il file system in uso e 
//  mantiene memoria delle risorse aperte
typedef struct OFSStruct_t {
    DEVICE    * dev;

    OFSBoot_t * boot;
    OFSPtr_t  * fat;
    uint64_t    fat_size;   // Bytes
    uint64_t    cls_bytes;

    off_t    cls_dentries;

    OFSFile_t * root_dir;

    Array_t *   fhmem;
    NumHT_t *   fomem;      // mappa di File Object
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


/*
 *  Alloca cnt cluster e ritorna la testa della catena
*/
OFSPtr_t ofsAllocClusters( OFS_t * ofs, size_t cnt );
/*
 *  Data una catena di cluster li dealloca
*/
void ofsDeallocClusters( OFS_t * ofs, OFSPtr_t clsHead );

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
 *  Cerca una dentry a partire dal nome del file all'interno 
 *  di una directory. Lo spazio per la dentry è allocato dalla
 *  funzione stessa e dovrà essere liberato trmite ofsFreeDentry
 *
 *  @param  ofs
 *  @param  dir     directory in cui cercare
 *  @param  fname   stringa rappresentate il nome del file
 *  @param  fnsize  dimensione del nome del file
 *
 *  @return     puntatore alla Dentry o NULL
 *
*/
OFSDentry_t * ofsGetDentry( OFS_t * ofs, OFSFile_t * dir, const char * fname, size_t fnsize );
void ofsFreeDentry( OFSDentry_t * dent );


/**
 *  Ottiene un file handle per la dentry specificata
 *  se il file dovesse avere OFS_FLAG_INVALID attiva
 *  allora l'handle non verrà generato.
 *  Il file così aperto verrà salvato nella memoria
 *  dei file aperti.
 *
 *  @param  dentry      Puntatore alla dentry che rappresenta
 *                      il file
 *
 *  @return             Ritorna un puntatore all'handle o NULL
*/
OFSFile_t * ofsOpenFile( OFS_t * ofs, OFSDentry_t * dentry );

/**
 *  Se non il file non ha riferimenti attivi, lo rimuove
 *  dalla memoria dei file aperti e libera le sue risorse.
 *  Se il file dovesse coincidere con la root directory
 *  memorizzata in 'ofs', la risorsa non verrà chiusa.
 *
 *  @param  file    Oggetto file
*/
void ofsCloseFile( OFS_t * ofs,  OFSFile_t * file );

/**
 *  Ottiene un file precedentemente aperto identificato dalla 
 *  key.
 *  @param  key     Chiave d'identificazione del file
 *                  nella memoria dei file aperti
 *
 *  @return     Puntatore al file o NULL
*/
OFSFile_t * ofsGetFile( OFS_t * ofs, NumHTKey_t key );

/**
 *  Alloca lo spazio per un nuovo file e restituisce una
 *  dentry di riferimento al file.
 *  Il file creato avrà allocato un solo cluster e size zero,
 *  se sia una directory ad essere creata il cluster viene azzerato.
 *  Il file creato non appartiene a nessuna directory,
 *  l'assegnazione all'albero di directory dovrà essere
 *  svolta in un secondo momento.
 *  Come per ofsGetDentry(), la dentry dovrà essere liberata
 *  tramite ofsFreeDentry().
 *
 *  @param  fname       nome del file
 *  @param  fnsize      dimensione in byte del nome file
 *  @param  flags       flag del file
 *
 *  @return     Puntatore alla dentry o NULL
*/
OFSDentry_t * ofsCreateEmptyFile( OFS_t * ofs, const char * fname, size_t fnsize, OFSFlags_t flags );


/**
 *  Apre una handle sul file specificato e la registra nella
 *  memoria delle handle aperte; questa operazione incrementa
 *  il numero di riferimenti al file indicato impedendone la
 *  chiusura.
 *
 *  @param  file    File di cui aprire l'handle
 *
 *  @return     puntatore alla handle o NULL
*/
OFSFileHandle_t * ofsOpenFileHandle( OFS_t * ofs, OFSFile_t * file );

/**
 *  Chiude la handle, rimuovendola dalla
 *  memoria delle handle aperte e riducendo i riferimenti al
 *  file associato.
 *
 *  @param  fh      Handle che deve essere distrutta
*/
void ofsCloseFileHandle( OFS_t * ofs, OFSFileHandle_t * fh );

/**
 *  Ottiene la handle identificata dall'indice
*/
OFSFileHandle_t * ofsGetFileHandle( OFS_t * ofs, off_t idx );