

#include "common.h"




int ofs_access (const char * path, int req) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs = fc->private_data;
    OFSFile_t * file    = NULL;
    int errcode = 0;

    file = ofsGetPathFile(ofs, path);
    if ( ! file && ( req & F_OK ) ) cleanup_errno(EACCES);
    if ( file == NULL ) cleanup_errno(ENOENT);

    if ( (file->flags & OFS_FLAG_RDONLY) && (req & W_OK) )
        cleanup_errno(EACCES);

    return 0;

cleanup:
    return -errcode;
}


int ofs_getattr(const char * path, struct stat *stbuf, struct fuse_file_info *fi) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs = fc->private_data;
    OFSFile_t * file = NULL;
    int errcode = 0;
    bool fileOpen = false;

    if ( fi != NULL )
        fileOpen = true;

    if ( ! fileOpen )
        file = ofsGetPathFile(ofs, (char *) path);
    else
        file = ofsGetFileHandle(ofs, fi->fh);

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


    if ( ! fileOpen )
        ofsCloseFile(ofs, file);

    return errcode;

cleanup:

    if ( (! fileOpen) && file )
        ofsCloseFile(ofs, file);

    return -errcode;
}


