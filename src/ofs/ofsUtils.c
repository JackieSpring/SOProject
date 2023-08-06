

#include "ofsUtils.h"





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
    
    if ( dir->last_entry_index == (ofs->cls_dentries - 1) ){
        if ( ofsExtendFile(ofs, file) )
            goto cleanup;
        else
            dir->last_entry_index = OFS_DIR_NO_ENTRIES;
    }

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

    if ( directory->entries && (directory->last_entry_index == 0) ){ // Restringe il file solo se avanzano entry
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




int ofsExtendFile( OFS_t * ofs, OFSFile_t * file ) {
    return ofsExtendFileBy(ofs, file, 1);
}


int ofsExtendFileBy( OFS_t * ofs, OFSFile_t * file, size_t cnt ) {
    if ( ! ( ofs && file ) )
        return -1;

    OFSPtrList_t * pList = file->cls_list;
    OFSPtr_t freeHead   = OFS_INVALID_CLUSTER;
    OFSPtr_t cls_ptr    = OFS_INVALID_CLUSTER;
    size_t idx          = 0;

    freeHead = ofsAllocClusters(ofs, cnt);
    if ( freeHead == OFS_INVALID_CLUSTER )
        goto cleanup;
    
    ofs->fat[ pList->tail ] = freeHead;

    idx = 0;
    cls_ptr = freeHead;
    for ( ; idx < cnt && cls_ptr != OFS_LAST_CLUSTER; idx++, cls_ptr = ofs->fat[cls_ptr] ) {

        if ( appendItem(pList, cls_ptr) == OFS_INVALID_CLUSTER )
            goto cleanup;

        //  Il cluster potrebbe essere sporco falsando
        //  struttura della directory
        if ( file->flags & OFS_FLAG_DIR ) {
            OFSCluster_t * cls = ofsGetCluster(ofs, cls_ptr);
            if ( ! cls )
                goto cleanup;

            memset( cls->data, '\0', cls->size );

            ofsFreeCluster(cls);
        }
    }

    return 0;

cleanup:

    for ( ; idx + 1 > 0 ; idx-- )
        popItem(pList);

    if ( cls_ptr )
        ofsDeallocClusters(ofs, cls_ptr);

    return -1;

}

int ofsShrinkFile( OFS_t * ofs, OFSFile_t * file ) {
    if ( ! ( ofs && file ) )
        return -1;

    OFSPtrList_t * pList= file->cls_list;
    OFSPtr_t tail;

    tail = popItem(pList);
    if ( tail == OFS_INVALID_CLUSTER )
        return -1;

    ofsDeallocClusters(ofs, tail);

    if ( pList->size > 0 )
        ofs->fat[ pList->tail ] = OFS_LAST_CLUSTER;


    return 0;

}

int ofsShrinkFileBy( OFS_t * ofs, OFSFile_t * file, size_t cnt ) {
    if ( ! ( ofs && file ) )
        return -1;

    if ( ! cnt )
        return 0;

    OFSPtrList_t * pList= file->cls_list;
    OFSPtr_t tail = OFS_RESERVED_CLUSTER;
    OFSPtr_t pArr[cnt];
    size_t i = 0;

    for ( ; i < cnt && tail != OFS_INVALID_CLUSTER; pArr[i] = tail = popItem(pList), i++ );

    if ( tail == OFS_INVALID_CLUSTER )
        return -1;

    ofsDeallocClusters(ofs, tail);

    if ( pList->size > 0 )
        ofs->fat[ pList->tail ] = OFS_LAST_CLUSTER;

    return 0;

cleanup:

    for ( i-- ; i + 1 > 0 ; appendItem(pList, pArr[i]) );

    return -1;
}



OFSFile_t * ofsGetParent( OFS_t * ofs, const char * path ) {

    OFSFile_t * ret = NULL;
    char  * copy    = NULL;
    char * filename = NULL;

    copy = strndup( path, strlen(path) );
    filename = &copy[ strlen(path) ];
    for ( ; *filename != '/' ; filename--  );
    filename++;
    *filename = '\0';

    ret = ofsGetPathFile(ofs, copy);
    if ( ! ret )
        goto cleanup;

    free(copy);

    return ret;

cleanup:
    if ( copy )
        free(copy);

    return NULL;
}