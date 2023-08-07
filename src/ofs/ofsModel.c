
#include "ofsModel.h"


/*

// IL MODELLO DEL FILE SYSTEM

La struttura OFS_t rappresenta un file system aperto e referenzia quattro
strutture fondamentali:
    - il boot cluster
    - la tabella fat
    - Array dei file handle
    - Mappa dei file aperti

All'apertura del file system il cluster di boot e la tabella fat vengono
recuperati dal device e mappati in memoria e lo resteranno fino alla
chiusura del file system; successivamente vengono creati l'Array dei file
handle e la mappa dei file aperti. Il primo file a venire registrato nella
mappa è la root directory, nel caso in cui la root directory ancora non
esista nel file system, viene creata al volo. La root directory è un file
che gode di permessi speciali, è l'unico file che non può essere chiuso
e privo di un parent.

// FILE vs FILE HANDLE

Le strutture file e i file handle sono entità distinte, per manipolare 
un file non è necessario un file handle, ma è necessaria un struttura
file; in ogni momento può esistere una sola copia di una struttura file
memorizzata nella mappa dei file, ma potrebbe avere molteplici o nessun
file handle che la referenziano.
Questo perché mentre le strutture file sono rappresentazioni dei file
sul device, i file handle sono dei riferimenti ad uso esclusivo dei
processi.
Nella mappa dei file aperti, essi sono identificati dal loro primo
cluster, mentre i file handle sono indicizzati dalla loro posizione
nell'array dei file handle; questo indice viene usato dagli hook per
recuperare il file handle.
Nonostante queste differenze un file è dipendente dai suoi file handle
per quanto concerne la sua chiusura, non è possibile chiudere un file
che abbia dei file handle attivi su di se.

// IL MODELLO FILE

La struttura OFSFile_t possiede quattro campi fondamentali:
    - dati del file
    - chiave di riconoscimento nella mappa dei file
    - lista dei cluster
    - numero di riferimenti

I dati sono recuperati dalla dentry passata a ofsFileOpen e sono
    - nome
    - numero di byte del nome
    - dimensione del file (0 per le directory)
    - flag del file

La lista dei cluster è una semplice lista degli indici dei cluster
e non di oggetti OFSCluster_t come si potrebbe pensare, questa
lista viene costruita a partire dal primo cluster usando la tabella
fat; dopo la creazione i dati della lista sono indipendenti dalla
tabella fat, pertanto ad ogni modifica della tabella è anche
necessaria una modifica della lista.
La chiave che identifica il file nella mappa non è altro che il primo
cluster della lista, questa scelta è giudata dalla semplicità nella
realizzazione della mappa e dall'impossibilità di creare un file
privo di cluster.
Il numero di riferimenti tiene traccia del numero di file handle 
attivi sul file e viene incrementato/decrementato dalle chiamate a
ofsOpenFileHandle / ofsCloseFileHandle .

// IL MODELLO DIRECTORY

Il modello directory è una sottoclasse del file, possiede due campi
aggiuntivi, ma il più importante è last_entry_index.
Il campo last_entry_index memorizza l'indice dell'ultima entry
occupata nella directory rispetto all'ultimo cluster; questa
informazione è rilevante per i seguenti motivi:
(1) Efficienza
    Conoscendo l'indice dell'ultima dentry non è necessario
    esplorare tutto il cluster per trovare una dentry (che potrebbe 
    anche essere molto esteso)

(2) Frammentazione
    Gli algoritmi di inserimento / rimozione riducono la frammentazione
    interna operando solo sull'ultima dentry della directory,
    "accodando" durante gli inserimenti e "riducendo" duranet le rimozioni.


*/



OFS_t * ofsOpen( DEVICE * dev ) {
    if ( dev == NULL )
        goto cleanup;


    OFS_t * ofs         = NULL;
    OFSBoot_t * boot    = MAP_FAILED;
    Array_t * fhmem     = NULL;
    NumHT_t * fomem     = NULL;



    ofs = (OFS_t *) calloc( 1, sizeof(OFS_t) );
    if ( ofs == NULL )
        goto cleanup;

    ofs->dev = dev;
    ofs->boot = MAP_FAILED;
    ofs->fat = MAP_FAILED;
    ofs->fat_size = 0;
    ofs->root_dir = NULL;

    // Mappa in memoria l'intero boot sector del file system
    // in modo da potervi facilemnte accedere come struttura

    boot = mmap( NULL, sizeof(OFSBoot_t), PROT_READ | PROT_WRITE, MAP_SHARED, dev->dd, 0 );
    if ( boot == MAP_FAILED )
        goto cleanup;
    ofs->boot = boot;

    // Mappa in memoria la tabella FAT, poiché i settori mappati
    // essere allineati con le pagine di memoria e le dimensioni
    // di questi due potrebbero non coincidere sempre, prima
    // calcolo l'offset dall'allineamento "piu vicino", poi 
    // alloco la sezione richiesta e infine ignoro i dati in eccesso

    OFSSec_t fatStart = ofs->boot->fat_sec;
    OFSSec_t fatEnd = ofs->boot->first_sec;
    OFSSecSize_t secSize = ofs->boot->sec_size;
    off_t ovf;
    uint8_t * fptr  = MAP_FAILED;
    uint64_t fat_size = (fatEnd - fatStart) * secSize;

    ovf = (fatStart * secSize) % SYS_PAGE_SIZE;

    fptr = mmap( NULL, fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev->dd, (fatStart * secSize) - ovf );
    if ( fptr == MAP_FAILED )
        goto cleanup;

    ofs->fat = (OFSPtr_t *) (fptr + ovf);
    ofs->fat_size = fat_size;
    ofs->cls_bytes = ofs->boot->sec_size * ofs->boot->cls_size;
    ofs->cls_dentries = ofs->cls_bytes / sizeof( OFSDentry_t );

    // FHMEM

    fhmem = arrayCreate(OFS_FHMEM_INIT_SIZE);
    if ( ! fhmem )
        goto cleanup;

    ofs->fhmem = fhmem;

    // FOMEM

    fomem = numHTCreate();
    if ( ! fomem )
        goto cleanup;

    ofs->fomem = fomem;


    OFSDentry_t dummyDentry = {0};
    OFSDentry_t * rootDentry = NULL;

    if ( ofs->boot->root_dir_ptr == OFS_INVALID_CLUSTER ) {
        rootDentry = ofsCreateEmptyFile(ofs, NULL, 0, OFS_FLAG_DIR);
        if ( ! rootDentry )
            goto cleanup;
    }
    else {
        dummyDentry.file_first_cls = ofs->boot->root_dir_ptr;
        dummyDentry.file_flags = OFS_FLAG_DIR;
        dummyDentry.file_name[0] = '\0';
        dummyDentry.file_name_size = 0;
        dummyDentry.file_size = 0;

        rootDentry = &dummyDentry;
    }

    ofs->root_dir = ofsOpenFile(ofs, rootDentry);
    if ( ofs->root_dir == NULL )
        goto cleanup;

    if ( ofs->boot->root_dir_ptr == OFS_INVALID_CLUSTER ) {
        ofs->boot->root_dir_ptr = rootDentry->file_first_cls;
        ofsFreeDentry(rootDentry);
    }

    
    return ofs;

cleanup:

    if ( ofs )
        free(ofs);

    if ( boot != MAP_FAILED )
        munmap( boot, sizeof(OFSBoot_t) );

    if ( fptr != MAP_FAILED )
        munmap( fptr, fat_size );

    if ( fhmem )
        arrayDestroy(fhmem);

    if ( fomem )
        numHTDestroy(fomem);

    return NULL;
}

void ofsClose( OFS_t * ofs ){

    if ( ! ofs)
        return;

    OFSFile_t * root = ofs->root_dir;

    ofs->root_dir = NULL;

    ofsCloseFile(ofs, root);
    arrayDestroy(ofs->fhmem);
    numHTDestroy(ofs->fomem);
    munmap( ofs->fat, ofs->fat_size );
    munmap( ofs->boot, sizeof(OFSBoot_t) );
    free(ofs);

    return;
}





/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  CLUSTER OPS
//

OFSCluster_t * ofsGetCluster( OFS_t * ofs, OFSPtr_t ptr ) {

    if ( ptr == OFS_RESERVED_CLUSTER || ptr == OFS_INVALID_CLUSTER )
        return NULL;

    OFSCluster_t * cls = NULL;
    OFSSec_t clsStartSec = ofsClusterToSector( ofs->boot, ptr ) ;
    off_t ovf = ofsSectorToByte( ofs->boot, clsStartSec ) & (off_t) MASK_SYS_PAGE_SIZE ;


    //cls = (OFSCluster_t * ) calloc( 1, sizeof(OFSCluster_t) );
    cls = malloc( sizeof( OFSCluster_t ) );
    if ( cls == NULL )
        return NULL;

    cls->cls_number = ptr;
    cls->next = ofs->fat[ptr];
    cls->size = ofs->cls_bytes;

    cls->data = mmap( NULL, ofs->cls_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, ofs->dev->dd, ofsSectorToByte( ofs->boot, clsStartSec ) - ovf );
    if ( cls->data == MAP_FAILED )
        goto cleanup;

    cls->data += ovf;

    return cls;

cleanup:

    if ( cls )
        free(cls);

    return NULL;
}


void ofsFreeCluster( OFSCluster_t * cls ) {
    if ( cls ) {
        off_t ovf = (off_t)cls->data & (off_t)MASK_SYS_PAGE_SIZE ;
        munmap(( (uint8_t *) cls->data) - ovf, cls->size);
        free( cls );
    }
}


OFSPtr_t ofsAllocClusters( OFS_t * ofs, size_t cnt ) {
    if ( ! ( ofs && cnt ) )
        return OFS_INVALID_CLUSTER;

    OFSBoot_t * boot    = ofs->boot;
    OFSPtr_t * fat      = ofs->fat;
    OFSPtr_t freeHead;
    OFSPtr_t last;

    if ( boot->free_cls_cnt < cnt )
        goto cleanup;

    freeHead = boot->free_ptr;
    last = freeHead;

    for ( size_t i = 1 ; i < cnt ; i++, last = fat[last] );

    boot->free_cls_cnt -= cnt;
    boot->free_ptr = fat[last];
    fat[last] = OFS_LAST_CLUSTER;

    return freeHead;

cleanup:
    return OFS_INVALID_CLUSTER;
}

void ofsDeallocClusters( OFS_t * ofs, OFSPtr_t clsHead ) {

    if ( ! ofs )
        return;

    if (clsHead == OFS_INVALID_CLUSTER ||
        clsHead == OFS_RESERVED_CLUSTER ||
        clsHead == OFS_LAST_CLUSTER )
        return;

    OFSBoot_t * boot    = ofs->boot;
    OFSPtr_t * fat      = ofs->fat;
    OFSPtr_t freeHead;
    OFSPtr_t last;
    size_t clsCnt = 1;

    freeHead = boot->free_ptr;
    last = clsHead;

    for (  ; fat[last] != OFS_LAST_CLUSTER ; clsCnt++, last = fat[last] );

    fat[last] = boot->free_ptr;
    boot->free_ptr = clsHead;
    boot->free_cls_cnt += clsCnt;

    return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  DENTRY OPS
//


OFSDentry_t * ofsGetDentry( OFS_t * ofs, OFSFile_t * dir, const char * fname, size_t fnsize ) {

    if ( ! ( ofs && dir ) )
        return NULL;

    if ( dir->flags & OFS_FLAG_INVALID )
        return NULL;

    if ( ! (dir->flags & OFS_FLAG_DIR) )
        return NULL;

    OFSCluster_t * cls;
    OFSDentry_t * sample;
    OFSDentry_t * ret;
    OFSDentry_t * dentryList;
    OFSPtrList_t * ptrList;
    bool flagFound = false;

    ptrList = dir->cls_list;

    ret = (OFSDentry_t *) calloc(1, sizeof(OFSDentry_t));
    if ( ret == NULL )
        goto cleanup;

    // Esplora ogni cluster della directory come array di Dentry
    // confronta il nome di ogni dentry con il nome cercato
    // al termine dell'esplorazione libera il cluster e passa
    // al successivo

    for ( OFSPtr_t i = 0; (i < ptrList->size) && (flagFound == false) ; i++ ){
        
        cls = ofsGetCluster(ofs, getItem(ptrList, i) );

        if ( cls == NULL )
            goto cleanup;

        dentryList = ( OFSDentry_t * ) cls->data;

        for ( uint64_t j = 0; j < (cls->size / sizeof(OFSDentry_t) ); j++ ){

            sample = &dentryList[j];

            if ( sample->file_flags == OFS_FLAG_FREE )
                continue;


            if (( fnsize != sample->file_name_size ) ||                                // Stessa lunghezza
                ( strncmp(fname, sample->file_name, sample->file_name_size) != 0 ) ||  // stessi caratteri
                ( sample->file_flags & OFS_FLAG_INVALID )                              // non invalid
            )
                continue;

            flagFound = true;

            memcpy( ret, sample, sizeof(OFSDentry_t));

            break;
        }

        ofsFreeCluster(cls);
    }

    if ( flagFound == false )
        goto cleanup;

    return ret;

cleanup:

    if ( ret )
        free(ret);

    return NULL;
}


void ofsFreeDentry( OFSDentry_t * dent ) {
    if ( ! dent )
        return;
    
    free( dent );
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  FILE OPS
//


OFSDentry_t * ofsCreateEmptyFile( OFS_t * ofs, const char * fname, size_t fnsize, OFSFlags_t flags ) {
    if ( ! ofs )
        return NULL;

    if ( fnsize > OFS_FILENAME_SAMPLE_SIZE )
        return NULL;

    if ( flags == OFS_FLAG_FREE )
        return NULL;


    OFSDentry_t * ret   = NULL;
    OFSCluster_t * cls  = NULL;
    OFSPtr_t cptr       = OFS_INVALID_CLUSTER;

    ret = calloc( 1, sizeof(OFSDentry_t) );
    if ( ! ret )
        goto cleanup;

    cptr = ofsAllocClusters(ofs, 1);
    if ( cptr == OFS_INVALID_CLUSTER )
        goto cleanup;

    if ( flags & OFS_FLAG_DIR ) {
        cls = ofsGetCluster(ofs, cptr);
        if ( ! cls )
            goto cleanup;
        memset( cls->data, 0, cls->size );
        ofsFreeCluster(cls);
        cls = NULL;
    }

    if ( fname )
        memcpy( &ret->file_name, fname, fnsize );
    ret->file_name_size = fnsize;
    ret->file_flags = flags;
    ret->file_size = 0;
    ret->file_first_cls = cptr;

    return ret;

cleanup:

    if ( cptr != OFS_INVALID_CLUSTER )
        ofsDeallocClusters(ofs, cptr);

    if ( cls )
        ofsFreeCluster(cls);

    if ( ret )
        free(ret);

    return NULL;
}


OFSFile_t * ofsOpenFile( OFS_t * ofs, OFSDentry_t * dentry ) {

    if ( !( ofs && dentry ) )
        return NULL;

    if ( dentry->file_flags & OFS_FLAG_INVALID )
        return NULL;

    OFSFile_t * ret         = NULL;
    OFSFileName_t * name    = NULL;
    OFSPtrList_t * clist    = NULL;
    NumHTKey_t fomem_key    = OFS_INVALID_CLUSTER;
    unsigned long objSize   = 0;

    if ( (ret = numHTGet(ofs->fomem, dentry->file_first_cls)) )
        return ret;
    else
        ret = NULL;


    if ( dentry->file_flags & OFS_FLAG_DIR )
        objSize = sizeof( OFSDir_t );
    else
        objSize = sizeof( OFSFile_t );

    ret = (OFSFile_t *) calloc( 1, objSize );
    if ( ! ret )
        goto cleanup;


    name = (OFSFileName_t *) calloc( 1, dentry->file_name_size );
    if ( name == NULL )
        goto cleanup;

    memcpy(name, dentry->file_name, dentry->file_name_size);

    
    clist = createList(ofs->fat, dentry->file_first_cls);
    if ( clist == NULL )
        goto cleanup;


    fomem_key = dentry->file_first_cls;
    if ( numHTInsert(ofs->fomem, fomem_key, ret) )
        goto cleanup;

    ret->name       = name;
    ret->cls_list   = clist;
    ret->name_size  = dentry->file_name_size;
    ret->flags      = dentry->file_flags;
    ret->size       = dentry->file_size;
    ret->fomem_key  = fomem_key;
    ret->refs       = 0;

    // Se directory, inizializza l'oggetto
    if ( ret->flags & OFS_FLAG_DIR ) {
        OFSDir_t * dret = (OFSDir_t *)ret;
        OFSCluster_t * cls = NULL;
        OFSDentry_t * dList;
        OFSDentry_t * sample;
        off_t    ecnt = 0;

        cls = ofsGetCluster(ofs, ret->cls_list->tail);
        if ( cls == NULL )
            goto cleanup;
        dList = (OFSDentry_t *) cls->data;

        // Conta le entry occupate nell'ultimo cluster
        // le dentry sono disposte ammassate in cima al
        // cluster mentre in fondo rimangono quelle libere
        for ( ; ecnt < ofs->cls_dentries && (dList[ecnt].file_flags != OFS_FLAG_FREE) ; ecnt++ );

        dret->entries = ((ret->cls_list->size - 1) * ofs->cls_dentries ) + ecnt;
        dret->last_entry_index = ecnt - 1;
        
    }

    return ret;
    

cleanup:

    if ( name )
        free(name);

    if ( clist )
        destroyList(clist);

    if ( fomem_key != OFS_INVALID_CLUSTER )
        numHTRemove(ofs->fomem, fomem_key);

    if (ret)
        free(ret);

    return NULL;
}


void ofsCloseFile( OFS_t * ofs,  OFSFile_t * file ) {
    if ( ! file )
        return;

    if ( file == ofs->root_dir )
        return;

    if ( file->refs > 0 )
        return;

    numHTRemove(ofs->fomem, file->fomem_key);
    destroyList(file->cls_list);
    free( file->name );
    free(file);

    return;
}


OFSFile_t * ofsGetFile( OFS_t * ofs, NumHTKey_t key ) {
    if ( ! ofs )
        return NULL;
    
    return numHTGet(ofs->fomem, key);
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  FILE HANDLE OPS
//



OFSFileHandle_t * ofsOpenFileHandle( OFS_t * ofs, OFSFile_t * file ) {
    if ( ! ( ofs && file ) )
        return NULL;

    OFSFileHandle_t * ret;
    off_t idx;

    ret = calloc( 1, sizeof( OFSFileHandle_t ) );
    if ( ! ret )
        goto cleanup;

    idx = arrayInsert(ofs->fhmem, ret);
    if ( idx == ARRAY_ITEM_INVALID )
        goto cleanup;

    ret->fhmem_idx = idx;
    ret->fomem_idx = file->fomem_key;
    ret->open_flags= 0;
    ret->seek_ptr  = 0;

    file->refs++;

    return ret;

cleanup:

    if ( ret )
        free(ret);

    return NULL;
}


void ofsCloseFileHandle( OFS_t * ofs, OFSFileHandle_t * fh ) {
    if ( ! (ofs && fh) )
        return;

    OFSFile_t * file;

    file = numHTGet(ofs->fomem, fh->fomem_idx);
    if ( file )
        file->refs--;

    arrayRemove(ofs->fhmem, fh->fhmem_idx);
    free(fh);
}

OFSFileHandle_t * ofsGetFileHandle( OFS_t * ofs, off_t idx ) {
    if ( ! ofs )
        return NULL;

    if ( idx == ARRAY_ITEM_INVALID )
        return NULL;

    return arrayGet(ofs->fhmem, idx);
}
