

#include "directories.h"






typedef struct DirIteratorToolsStruct {
    OFS_t * ofs;
    OFSFile_t * dir;
    OFSDentry_t * dentry;
    void * buf;
    fuse_fill_dir_t filler;
} DirIteratorTools;


///////////////////////////////////////////////////////////////////////////////////////////
// ITERATORI


bool _ofsReadDirIterator( OFSDentry_t * dent, DirIteratorTools * dt ) {
    
    if ( dent->file_flags == OFS_FLAG_FREE )
        return true;

    if ( dent->file_flags & ( OFS_FLAG_HIDDEN | OFS_FLAG_INVALID ) )
        return true;

    char * name;

    name = calloc( dent->file_name_size + 1, sizeof( uint8_t ) );

    memcpy(name, dent->file_name, dent->file_name_size);
    dt->filler( dt->buf, name, NULL, 0, 0 );
    free( name );

    return true;
}



///////////////////////////////////////////////////////////////////////////////////////////
// HOOKS

int ofs_opendir (const char * path, struct fuse_file_info * fi) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs             = fc->private_data;
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fileH = NULL;
    int errcode = 0;


    file = ofsGetPathFile(ofs, path);
    if ( file == NULL )
        cleanup_errno(ENOENT);

    if ( ! (file->flags & OFS_FLAG_DIR) )
        cleanup_errno(ENOTDIR);

    fileH = ofsOpenFileHandle(ofs, file);
    if ( fileH == NULL )
        cleanup_errno(EIO);
    
    fi->fh = fileH->fhmem_idx;

    return 0;

cleanup:

    if ( file && file->refs < 1 )
        ofsCloseFile(ofs, file);

    return -errcode;
}

int ofs_releasedir(const char * path, struct fuse_file_info * fi) {


    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs             = fc->private_data;
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fileH = NULL;
    int errcode = 0;


    if ( fi == NULL )
        return 0;

    fileH = ofsGetFileHandle(ofs, fi->fh);
    if ( ! fileH )
        cleanup_errno(EBADF);
    
    file = ofsGetFile( ofs, fileH->fomem_idx ); // TODO!!!!!!!
    if ( file == NULL )
        cleanup_errno(EBADF);
    
    ofsCloseFileHandle(ofs, fileH);

    if ( file->refs < 1 )
        ofsCloseFile(ofs, file);

cleanup:
    return -errcode;
}


int ofs_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    struct fuse_context * fc;
    OFS_t * ofs             = NULL;
    OFSFile_t * dir         = NULL;
    OFSFileHandle_t * fileH = NULL;
    int errcode = 0;

    fc = fuse_get_context();
    ofs = fc->private_data;

    fileH = ofsGetFileHandle(ofs, fi->fh);
    if ( ! fileH )
        cleanup_errno(EBADF);

    dir = ofsGetFile(ofs, fileH->fomem_idx);
    if ( ! dir )
        cleanup_errno(ENOENT);

    filler( buf, ".", NULL, 0, 0 );
    filler( buf, "..", NULL, 0, 0 );

    DirIteratorTools dt;

    dt.buf = buf;
    dt.dentry = NULL;
    dt.filler = filler;
    dt.dir = dir;
    dt.ofs = ofs;

    if ( ofsDirectoryIterator(ofs, dir, _ofsReadDirIterator, &dt) )
        cleanup_errno(EIO);

    return 0;

cleanup:

    return -errcode;

}



int ofs_mkdir (const char * path, mode_t mode) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs         = fc->private_data;
    OFSFile_t * dir     = NULL;
    char * copy         = NULL;
    char * filename     = NULL;
    OFSDentry_t dent    = {0};
    size_t  pathLen     = 0;
    int errcode         = 0;
    register void * tool;


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

    
    // crea la nuova dentry
    dent.file_first_cls = ofsAllocClusters(ofs, 1);
    memcpy( &dent.file_name, filename, strlen(filename) );
    dent.file_name_size = strlen(filename);
    dent.file_size = 0;
    dent.file_flags = OFS_FLAG_DIR;

    if ( mode & O_RDONLY )
        dent.file_flags |= OFS_FLAG_RDONLY;

    if ( ofsInsertDentry(ofs, dir, &dent) )
        cleanup_errno(EIO);

    if ( dir->refs < 1 )
        ofsCloseFile(ofs, dir);

    return 0;


cleanup:

    if ( copy )
        free(copy);

    if ( dir && dir->refs < 1 )
        ofsCloseFile(ofs, dir);

    return -errcode;
}

int ofs_rmdir (const char * path) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs         = fc->private_data;
    OFSFile_t * file    = NULL;
    OFSDir_t * dir      = NULL;
    OFSDir_t * oldDir   = NULL;
    OFSDentry_t * dentry= NULL;
    OFSPtr_t head       = OFS_LAST_CLUSTER;
    size_t pathLen      = 0;
    char * copy         = NULL;
    char * filename     = NULL;
    int errcode         = 0;


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
    file = ofsGetPathFile(ofs, copy);
    if ( file == NULL )
        cleanup_errno(ENOTDIR);

    // è directory?
    if ( ! (file->flags & OFS_FLAG_DIR) )
        cleanup_errno(ENOTDIR);

    // permessi d'accesso?
    if ( file->flags & OFS_FLAG_RDONLY )
        cleanup_errno(EACCES);

    dir = (OFSDir_t * ) file;

    dentry = ofsGetDentry(ofs, file, filename, strlen(filename));
    if ( dentry == NULL )
        cleanup_errno(ENOENT);

    if ( ! (dentry->file_flags & OFS_FLAG_DIR) )
        cleanup_errno(ENOTDIR);

    oldDir = (OFSDir_t *) ofsOpenFile(ofs, dentry);
    if ( oldDir == NULL )
        cleanup_errno(EIO);

    if ( oldDir->entries > 0 )
        cleanup_errno(ENOTEMPTY);

    head = oldDir->super.cls_list->head;

    ofsDeleteDentry(ofs, dir, filename, strlen(filename));

    if ( file->refs < 1 )
        ofsCloseFile(ofs, file);
    
    ofsCloseFile(ofs, oldDir);
    ofsDeallocClusters(ofs, head);
    ofsFreeDentry(dentry);

    return 0;

cleanup:

    if ( copy )
        free(copy);

    if ( file && file->refs < 1 )
        ofsCloseFile(ofs, file);

    if ( dentry )
        ofsFreeDentry(dentry);

    return -errcode;

}


