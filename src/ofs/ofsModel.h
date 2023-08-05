
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
    OFSPtr_t        parent_dir;
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



typedef bool (* directory_iterator) ( OFSDentry_t * dentry, void * extra );




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
void ofsCloseFile( OFS_t * ofs,  OFSFile_t * file );

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
 *  Inserisce una dentry in una directory; la funzione non gestisce
 *  i casi di duplicati. La nuova dentry sarà una copia della dentry 
 *  passata tra gli argomenti.
 *
 *  @param  ofs 
 *  @param  file    directory in cui inserire la dentry
 *  @param  dent    dentry da inserire
 *
 *  @return     0 o -1 in caso di errore
 *
*/
int ofsInsertDentry( OFS_t * ofs, OFSFile_t * file, OFSDentry_t * dent );
void ofsDeleteDentry( OFS_t * ofs, OFSFile_t * dir, const char * fname, size_t fnsize );

/**
 *  Ottiene un file a partire dal percorso
*/
OFSFile_t * ofsGetPathFile( OFS_t * ofs, char * path );

/**
 *  Itera attraverso tutte le dentry di una directory
*/
int ofsDirectoryIterator( OFS_t * ofs, OFSFile_t * dir, directory_iterator callback, void * extra );



/*
 *  Estende o riduce un file di un cluster
*/
int ofsExtendFile( OFS_t * ofs, OFSFile_t * file );
int ofsShrinkFile( OFS_t * ofs, OFSFile_t * file );

/*
 *  Alloca cnt cluster e ritorna la testa della catena
*/
OFSPtr_t ofsAllocClusters( OFS_t * ofs, size_t cnt );
/*
 *  Data una catena di cluster li dealloca
*/
void ofsDeallocClusters( OFS_t * ofs, OFSPtr_t clsHead );


OFSFileHandle_t * ofsOpenFileHandle( OFS_t * ofs, OFSFile_t * file );
void ofsCloseFileHandle( OFS_t * ofs, OFSFileHandle_t * fh );
OFSFileHandle_t * ofsGetFileHandle( OFS_t * ofs, off_t idx );

OFSFile_t * ofsGetFile( OFS_t * ofs, NumHTKey_t key );