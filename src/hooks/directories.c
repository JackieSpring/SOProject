

#include "directories.h"




int ofs_opendir (const char * path, struct fuse_file_info * fi) {

    return 0;
}

int ofs_getattr(const char * path, struct stat *stbuf, struct fuse_file_info *fi) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs = fc->private_data;
    OFSFile_t * file = NULL;
    int errcode = 0;

    file = ofsGetPathFile(ofs, (char *) path);

    if ( file == NULL )
        cleanup_errno(ENOENT);


    memset( stbuf, 0, sizeof( struct stat ) );

    stbuf->st_dev = ofs->dev->dstat.st_dev;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();

    stbuf->st_mode  = S_IWUSR | S_IRUSR;
    stbuf->st_nlink = 1;

    stbuf->st_size      = file->size;
    stbuf->st_blksize   = ofs->boot->sec_size;
    stbuf->st_blocks    = (file->size / 512 );
    if ( ! stbuf->st_blksize )
        stbuf->st_blksize = 1;


    if ( file->flags & OFS_FLAG_DIR )
        stbuf->st_mode = S_IFDIR | S_IXUSR;

    if ( file->flags & OFS_FLAG_FILE )
        stbuf->st_mode = S_IFREG;

    if ( file->flags & OFS_FLAG_RDONLY )
        stbuf->st_mode &= S_IRUSR;


    if ( file != ofs->root_dir )
        ofsCloseFile(file);

    return errcode;

cleanup:

    if ( file )
        if ( file != ofs->root_dir )
            ofsCloseFile(file);

    return -errcode;
}





typedef struct DirIteratorToolsStruct {
    OFS_t * ofs;
    OFSFile_t * dir;
    OFSDentry_t * dentry;
    void * buf;
    fuse_fill_dir_t filler;
} DirIteratorTools;

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

int ofs_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

    OFS_t * ofs     = NULL;
    OFSFile_t * dir = NULL;
    int errcode;

    ofs = fuse_get_context();
    dir = ofsGetPathFile(ofs, path);

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

    ofsDirectoryIterator(ofs, dir, _ofsReadDirIterator, &dt);


    if ( dir != ofs->root_dir )
        ofsCloseFile(dir);
    

    return 0;

cleanup:

    if ( dir )
        if ( dir != ofs->root_dir )
            ofsCloseFile(dir);

    return -errcode;

}