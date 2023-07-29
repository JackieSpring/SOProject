

#include "directories.h"


int of_opendir (const char * path, struct fuse_file_info * fi){
    notifyMessage(logger, "OPENDIR %s\n", path);

    return 0;
}

int of_getattr(const char * path, struct stat *stbuf, struct fuse_file_info *fi) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs = fc->private_data;
    OFSFile_t * file = NULL;
    int errcode = 0;

    file = ofsGetPathFile(ofs, path);

    if ( file == NULL )
        return -ENOENT;
        //cleanup_errno(ENOENT);
    /*{
        notifyError(logger, "GETATTR err path %s\n", path);
        return -ENOENT;
    }*/

    //if ( file->flags & ( OFS_FLAG_HIDDEN | OFS_FLAG_INVALID ) )
    //    return -ENOENT;

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


    //if ( file != ofs->root_dir )
    //    ofsCloseFile(file);

    return errcode;

cleanup:

    if ( file )
        if ( file != ofs->root_dir )
            ofsCloseFile(file);

    return -errcode;
}



int of_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

    OFS_t * ofs = NULL;
    OFSFile_t * dir = NULL;
    int errcode;

    ofs = fuse_get_context();
    dir = ofsGetPathFile(ofs, path);

    printf("[[ NOME ]] %p\n", dir->name);

    if ( ! dir )
        cleanup_errno(ENOENT);

    filler( buf, ".", NULL, 0, 0 );
    filler( buf, "..", NULL, 0, 0 );

    if ( dir != ofs->root_dir )
        ofsCloseFile(dir);
    
    notifyMessage(logger, "READDIR %s\n", path);

    return 0;

cleanup:

    notifyError(logger, "ERRORE readdir %s\n", path );

    if ( dir )
        if ( dir != ofs->root_dir )
            ofsCloseFile(dir);

    return -errcode;

}