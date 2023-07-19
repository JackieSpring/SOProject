
#include "ofsModel.h"
#include "ofsStructures.h"
#include <string.h>



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
        free( cls );
    }
}



OFSFile_t * ofsOpenFile( OFS_t * ofs, OFSDentry_t * dentry ) {

    if ( !( ofs && dentry ) )
        return NULL;

    if ( dentry->file_flags & OFS_FLAG_INVALID )
        return NULL;

    OFSFile_t * ret;
    OFSFileName_t * name;
    OFSPtrList_t * clist;

    ret = (OFSFile_t *) calloc( 1, sizeof(OFSFile_t) );
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


void ofsCloseFile( OFSFile_t * file ) {
    if ( ! file )
        return;

    free( file->name );
    destroyList(file->cls_list);
    free(file);

    return;
}



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

        for ( uint64_t i = 0; i < (cls->size / sizeof(OFSDentry_t) ); i++ ){
            sample = &dentryList[0];

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

