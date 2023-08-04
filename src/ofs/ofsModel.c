
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
    ofs->root_dir = NULL;   // TODO

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
    off_t ovf;
    uint8_t * fptr;

    ofs->fat_size = (fatEnd - fatStart) * secSize;
    ovf = (fatStart * secSize) % SYS_PAGE_SIZE;

    fptr = mmap( NULL, ofs->fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev->dd, (fatStart * secSize) - ovf );
    if ( fptr == MAP_FAILED )
        goto cleanup;

    ofs->fat = (OFSPtr_t *) (fptr + ovf);
    ofs->cls_bytes = ofs->boot->sec_size * ofs->boot->cls_size;
    ofs->cls_dentries = ofs->cls_bytes / sizeof( OFSDentry_t );

    OFSDentry_t rootDentry = {
        .file_first_cls = ofs->boot->root_dir_ptr,
        .file_flags = OFS_FLAG_DIR,
        .file_name = '\0',
        .file_name_size = 0,
        .file_size = 0,
    };

    ofs->root_dir = ofsOpenFile(ofs, &rootDentry);
    if ( ofs->root_dir == NULL )
        goto cleanup;

    
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



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  CLUSTER OPS
//

OFSCluster_t * ofsGetCluster( OFS_t * ofs, OFSPtr_t ptr ) {

    if ( ptr == OFS_RESERVED_CLUSTER || ptr == OFS_INVALID_CLUSTER )
        return NULL;

    OFSCluster_t * cls = NULL;
    OFSSec_t clsStartSec = ofsClusterToSector( ofs->boot, ptr ) ;
    off_t ovf = ofsSectorToByte( ofs->boot, clsStartSec ) & (off_t) MASK_SYS_PAGE_SIZE ;


    cls = (OFSCluster_t * ) calloc( 1, sizeof(OFSCluster_t) );
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
        munmap(( (uint8_t *) cls) - ovf, cls->size);
        free( cls );
    }
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  FILE OPS
//

OFSFile_t * ofsOpenFile( OFS_t * ofs, OFSDentry_t * dentry ) {

    if ( !( ofs && dentry ) )
        return NULL;

    if ( dentry->file_flags & OFS_FLAG_INVALID )
        return NULL;

    OFSFile_t * ret;
    OFSFileName_t * name;
    OFSPtrList_t * clist;
    unsigned long objSize = 0;

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

    ret->name = name;
    ret->cls_list = clist;
    ret->name_size = dentry->file_name_size;
    ret->flags = dentry->file_flags;
    ret->size = dentry->file_size;
    ret->parent_dir = NULL; // TODO

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

    if (ret)
        free(ret);

    return NULL;
}


void ofsCloseFile( OFS_t * ofs,  OFSFile_t * file ) {
    if ( ! file )
        return;

    if ( file == ofs->root_dir )
        return;

    free( file->name );
    destroyList(file->cls_list);
    free(file);

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


int ofsInsertDentry( OFS_t * ofs, OFSFile_t * file, OFSDentry_t * dent ) {

    if ( ! ( ofs && file && dent ) )
        return -1;

    if ( file->flags & OFS_FLAG_INVALID )
        return -1;

    if ( ! (file->flags & OFS_FLAG_DIR) )
        return -1;

    OFSDir_t * dir = (OFSDir_t *) file;
    OFSCluster_t * cls;
    OFSDentry_t * sample;
    OFSDentry_t * dlist;
    OFSPtrList_t * plist;

    plist = file->cls_list;
    
    if ( dir->last_entry_index == (ofs->cls_dentries - 1) )
        if ( ofsExtendFile(ofs, file) )
            goto cleanup;
        else
            dir->last_entry_index = OFS_DIR_NO_ENTRIES;

    cls = ofsGetCluster(ofs, plist->tail);
    dlist = (OFSDentry_t *)cls->data;
    sample = &dlist[ dir->last_entry_index + 1 ];

    memcpy(sample, dent, sizeof(OFSDentry_t));

    dir->entries++;
    dir->last_entry_index++;

    return 0;

cleanup:
    return -1;
}


bool _ofsDeleteDentryIterator ( OFSDentry_t * dentry, void * extra [5] ) {
    OFSPtr_t * dCls = (OFSPtr_t *) extra[0] ;
    off_t * dIdx = (off_t *) extra[1];
    char * fname = (char *) extra[2];
    size_t * fnsize = (size_t * ) extra[3];
    OFS_t * ofs = (OFS_t *) extra[4];
    bool * flagFound = (bool *) extra[5];

    *dIdx += 1;

    if ( *dIdx >= ofs->cls_dentries ) {
        *dCls += 1;
        *dIdx = 0;
    }


    if ( dentry->file_flags == OFS_FLAG_FREE )
        return true;

    if ( dentry->file_flags & ( OFS_FLAG_INVALID | OFS_FLAG_HIDDEN ) )
        return true;

    if ( dentry->file_name_size != *fnsize )
        return true;

    if ( strncmp( dentry->file_name, fname, dentry->file_name_size ) != 0 )
        return true;

    *flagFound = true;

    return false;
}

//  Cerca la dentry da rimuovere e la sovrascrive
//  l'ultima dentry della directory, poi cancella
//  l'ultima dentry.
//  Se si svuota un cluster a causa della rimozione
//  lo libera.
void ofsDeleteDentry( OFS_t * ofs, OFSFile_t * dir, const char * fname, size_t fnsize ) {

    if ( ! ( ofs && dir ) )
        return;

    if ( dir->flags & OFS_FLAG_INVALID )
        return;

    if ( ! (dir->flags & OFS_FLAG_DIR) )
        return;


    OFSDir_t * directory = (OFSDir_t *) dir;

    OFSPtr_t dCls = 0;
    off_t    dIdx = OFS_DIR_NO_ENTRIES;
    OFSCluster_t * dPage = NULL;

    OFSPtr_t lCls;
    off_t    lIdx;
    OFSCluster_t * lPage = NULL;

    OFSDentry_t * target = NULL;
    OFSDentry_t * last = NULL;
    OFSDentry_t zeros = {0};
    void * extra[6];
    bool flagFound = false;

    extra[0] = &dCls;
    extra[1] = &dIdx;
    extra[2] = fname;
    extra[3] = &fnsize;
    extra[4] = ofs;
    extra[5] = &flagFound;

    ofsDirectoryIterator(ofs, dir, _ofsDeleteDentryIterator , extra);

    if ( flagFound == false )
        return;

    dCls = getItem(dir->cls_list, dCls);
    if ( dCls == OFS_INVALID_CLUSTER )
        return;

    lCls = dir->cls_list->tail;
    lIdx = directory->last_entry_index;

    lPage = ofsGetCluster(ofs, lCls);
    if ( lPage == NULL )
        goto cleanup;


    if ( dCls != lCls )
        dPage = ofsGetCluster(ofs, dCls);
    else
        dPage = lPage;

    last = &((OFSDentry_t *)lPage->data)[lIdx];
    target = &((OFSDentry_t *)dPage->data)[dIdx];

    memcpy( target, last, sizeof(OFSDentry_t) );
    memcpy( last, &zeros, sizeof(OFSDentry_t));

    directory->entries--;

    if ( directory->entries && (directory->last_entry_index == 0) ){
        directory->last_entry_index = ofs->cls_dentries;
        ofsShrinkFile(ofs, dir);
    }else
        directory->last_entry_index--;

    ofsFreeCluster(lPage);

    if ( dCls != lCls )
        ofsFreeCluster(dPage);

cleanup:
    return;
}


void ofsFreeDentry( OFSDentry_t * dent ) {
    if ( ! dent )
        return;
    
    free( dent );
}


OFSFile_t * ofsGetPathFile( OFS_t * ofs, char * path ) {

    if ( ! ( path && ofs ) )
        return NULL;

    OFSFile_t * ret = NULL;
    OFSDentry_t * link = NULL;
    size_t arr_len = 0;
    char ** path_arr = NULL;

    if ( path[0] != '/' )
        goto cleanup;

    path += 1;  // ignora root

    path_arr = tokenize(path, '/');

    if ( path_arr == NULL )
        goto cleanup;

    for ( ; path_arr[arr_len] != NULL ; arr_len++ ) ;

    ret = ofs->root_dir;

    for ( size_t i = 0 ; i < arr_len ; i++ ) {

        // controlla che il nodo corrente sia una directory
        if ( ! ( ret->flags & OFS_FLAG_DIR ) )
            goto cleanup;

        link = ofsGetDentry(ofs, ret, path_arr[i], strlen(path_arr[i]));

        if ( link == NULL )
            goto cleanup;

        ofsCloseFile(ofs, ret);
        
        // apre il file e libera le risorse
        ret = ofsOpenFile(ofs, link);
        ofsFreeDentry(link);
    }

    free_str_arr(path_arr);

    return ret;

cleanup:

    if ( ret )
        ofsCloseFile(ofs, ret);

    if ( link )
        ofsFreeDentry(link);

    if ( path_arr )
        free_str_arr(path_arr);

    return NULL;
}




int ofsDirectoryIterator( OFS_t * ofs, OFSFile_t * dir, directory_iterator callback, void * extra ){

    if ( ! ( ofs && dir && callback ) )
        return -1;

    if ( dir->flags & OFS_FLAG_INVALID )
        return -1;

    if ( ! (dir->flags & OFS_FLAG_DIR) )
        return -1;
    

    OFSPtrList_t * pList    = NULL;
    OFSDentry_t * sample    = NULL;
    OFSDentry_t * dentryList= NULL;
    OFSCluster_t * cls      = NULL;
    bool flagRun;

    pList = dir->cls_list;
    flagRun = true;

    for ( OFSPtr_t cindex = 0 ; (cindex < pList->size) && flagRun ; cindex++ ) {
        cls = ofsGetCluster(ofs, getItem(pList, cindex));

        if ( cls == NULL )
            goto cleanup;

        dentryList = (OFSDentry_t *) cls->data;

        for ( uint64_t i = 0; i < (cls->size / sizeof(OFSDentry_t) ); i++ ) {
            if ( callback( &dentryList[i], extra ) )
                continue;

            flagRun = false;
            break;
        
        }

        ofsFreeCluster(cls);
    }


    return 0;

cleanup:

    return -1;

}




int ofsExtendFile( OFS_t * ofs, OFSFile_t * file ) {
    if ( ! ( ofs && file ) )
        return -1;

    if ( ofs->boot->free_cls_cnt < 1 )
        return -1;

    OFSBoot_t * boot = ofs->boot;
    OFSPtrList_t * pList = file->cls_list;
    OFSPtr_t freeHead = boot->free_ptr;

    boot->free_ptr = ofs->fat[freeHead];
    boot->free_cls_cnt--;

    ofs->fat[ pList->tail ] = freeHead;
    ofs->fat[freeHead] = OFS_LAST_CLUSTER;
    appendItem(pList, freeHead);


    return 0;
}

int ofsShrinkFile( OFS_t * ofs, OFSFile_t * file ) {
    if ( ! ( ofs && file ) )
        return -1;


    OFSBoot_t * boot    = ofs->boot;
    OFSPtrList_t * pList= file->cls_list;
    OFSPtr_t tail;

    tail = popItem(pList);
    if ( tail == OFS_INVALID_CLUSTER )
        return -1;
    
    ofs->fat[tail] = boot->free_ptr;
    boot->free_cls_cnt++;

    if ( pList->size > 0 )
        ofs->fat[ pList->tail ] = OFS_LAST_CLUSTER;


    return 0;

}



