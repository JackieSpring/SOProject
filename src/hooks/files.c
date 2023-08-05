

#include "files.h"
#include <stddef.h>
#include <sys/fcntl.h>



int ofs_create (const char * path, mode_t mode, struct fuse_file_info * fi ) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs             = fc->private_data;
    OFSFile_t * dir         = NULL;
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fh    = NULL;
    char * copy             = NULL;
    char * filename         = NULL;
    size_t pathLen          = 0;
    OFSDentry_t dent        = {0};
    int errcode = 0;
    OFSDentry_t * tool;


    // sufficiente spazio?
    if ( ofs->boot->free_cls_cnt < 1 )
        cleanup_errno(ENOSPC);

    // nome valido?
    pathLen = strlen(path);
    if ( pathLen < 1 )
        cleanup_errno(EILSEQ);

    filename = &path[pathLen];
    for ( ; *filename != '/' ; filename-- );
    filename++;


    copy = malloc( pathLen + 1 );
    if ( copy == NULL )
        cleanup_errno(EILSEQ);

    memcpy(copy, path, pathLen + 1);
    copy[ pathLen - strlen(filename) ] = '\0';

    // nome troppo lungo?
    if ( strlen(filename) > OFS_FILENAME_SAMPLE_SIZE )
        cleanup_errno(ENAMETOOLONG);

    // directory esiste?
    dir = ofsGetPathFile(ofs, copy);
    if ( dir == NULL )
        cleanup_errno(ENOENT);

    // è directory?
    if ( ! (dir->flags & OFS_FLAG_DIR) )
        cleanup_errno(EISDIR);

    // permessi d'accesso?
    if ( dir->flags & OFS_FLAG_RDONLY )
        cleanup_errno(EACCES);

    // il file esiste?
    if ( ( tool = ofsGetDentry(ofs, dir, filename, strlen(filename)) ) ){
        ofsFreeDentry(tool);
        cleanup_errno(EEXIST);
    }

    // mode è valido ?
    if ( ! mode )
        cleanup_errno(EINVAL);

    dent.file_first_cls = ofsAllocClusters(ofs, 1);
    dent.file_flags = OFS_FLAG_FILE;
    dent.file_size = 0;
    memcpy( &dent.file_name, filename, strlen(filename) );
    dent.file_name_size = strlen(filename);

    if ( ! (mode & 0x333) ) // Se creata con solo permessi di lettura
        dent.file_flags |= OFS_FLAG_RDONLY;

    if ( ofsInsertDentry(ofs, dir, &dent) ) // TODO Memory leak in caso di fallimento (non libera i cluster)
        cleanup_errno(EIO);


    file = ofsOpenFile(ofs, &dent);
    if ( ! file )
        cleanup_errno(EIO);

    fh = ofsOpenFileHandle(ofs, file);
    if ( ! fh )
        cleanup_errno(EIO);

    fi->fh = fh->fhmem_idx;

    ofsCloseFile(ofs, dir);

    return 0;

cleanup:

    if ( dir )
        ofsCloseFile(ofs, dir);

    if ( copy )
        free(copy);

    return -errcode;
}

int ofs_unlink (const char * path) {
    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs             = fc->private_data;
    OFSFile_t * dir         = NULL;
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fh    = NULL;
    char * copy             = NULL;
    char * filename         = NULL;
    size_t pathLen          = 0;
    OFSDentry_t  * dent     = NULL;
    int errcode = 0;
    OFSDentry_t * tool;


    // nome valido?
    pathLen = strlen(path);
    if ( pathLen < 1 )
        cleanup_errno(EILSEQ);

    filename = &path[pathLen];
    for ( ; *filename != '/' ; filename-- );
    filename++;


    copy = malloc( pathLen + 1 );
    if ( copy == NULL )
        cleanup_errno(EILSEQ);

    memcpy(copy, path, pathLen + 1);
    copy[ pathLen - strlen(filename) ] = '\0';

    // nome troppo lungo?
    if ( strlen(filename) > OFS_FILENAME_SAMPLE_SIZE )
        cleanup_errno(ENAMETOOLONG);

    // directory esiste?
    dir = ofsGetPathFile(ofs, copy);
    if ( dir == NULL )
        cleanup_errno(ENOENT);

    // è directory?
    if ( ! (dir->flags & OFS_FLAG_DIR) )
        cleanup_errno(EISDIR);

    // permessi d'accesso?
    if ( dir->flags & OFS_FLAG_RDONLY )
        cleanup_errno(EACCES);

    dent = ofsGetDentry(ofs, dir, filename, strlen(filename));
    if ( ! dent )
        cleanup_errno(ENOENT);

    file = ofsGetFile( ofs, dent->file_first_cls );
    if ( file && file->refs > 0 )
        cleanup_errno(EBUSY);

    ofsDeleteDentry(ofs, dir, filename, strlen(filename));
    ofsDeallocClusters(ofs, dent->file_first_cls);
    ofsFreeDentry(dent);
    ofsCloseFile(ofs, dir);


    return 0;

cleanup:

    if ( dir )
        ofsCloseFile(ofs, dir);

    return -errcode;
}



int ofs_open (const char * path, struct fuse_file_info * fi) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs = fc->private_data;
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fh    = NULL;
    off_t seek              = 0;
    int flagReq = fi->flags;
    int errcode = 0;


    file = ofsGetPathFile(ofs, path);
    if ( ! file )
        cleanup_errno(ENOENT);

    if ( file->flags & OFS_FLAG_DIR )
        cleanup_errno(EISDIR);

    if ( (flagReq & ( O_RDWR | O_WRONLY )) && (file->flags & OFS_FLAG_RDONLY) )
        cleanup_errno(EACCES);


    if ( flagReq & O_TRUNC ) { // Truncate size to 0
        if ( file->cls_list->size > 1 )
            ofsDeallocClusters(ofs, getItem(file->cls_list, 1));
        
        file->size = 0;
    }

    if ( flagReq & O_APPEND )   // imposta il cursore in fondo
        seek = file->size;

    fh = ofsOpenFileHandle(ofs, file);
    if ( ! fh )
        cleanup_errno(EIO);

    fh->open_flags = flagReq;
    fh->seek_ptr = seek;

    fi->fh = fh->fhmem_idx;

    return 0;

cleanup:

    if ( file )
        ofsCloseFile(ofs, file);

    return -errcode;
}

int ofs_release (const char * path, struct fuse_file_info * fi) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs = fc->private_data;
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fh    = NULL;
    int errcode = 0;

    fh = ofsGetFileHandle(ofs, fi->fh);
    if ( ! fh )
        cleanup_errno(EBADF);

    file = ofsGetFile(ofs, fh->fomem_idx);
    if ( ! file )
        cleanup_errno(EBADF);

    ofsCloseFileHandle(ofs, fh);
    ofsCloseFile(ofs, file);

    return 0;

cleanup:

    return -errcode;
}


int ofs_read (const char * path, char * buf, size_t size, off_t off, struct fuse_file_info * fi);
int ofs_write (const char * path, const char * buf, size_t size, off_t off, struct fuse_file_info * fi);
