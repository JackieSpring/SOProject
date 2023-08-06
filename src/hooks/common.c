

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
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fH    = NULL;
    int errcode = 0;
    bool fileOpen = false;

    if ( fi != NULL )
        fileOpen = true;

    if ( ! fileOpen )
        file = ofsGetPathFile(ofs, (char *) path);
    else {
        fH = ofsGetFileHandle(ofs, fi->fh);
        if ( ! fH )
            cleanup_errno(EBADF);
        
        file = ofsGetFile(ofs, fH->fomem_idx);
    }
    if ( file == NULL )
        cleanup_errno(ENOENT);


    memset( stbuf, 0, sizeof( struct stat ) );

    stbuf->st_dev = ofs->dev->dev;
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
        stbuf->st_mode = S_IFDIR ;

    if ( file->flags & OFS_FLAG_FILE )
        stbuf->st_mode = S_IFREG ;

    if ( file->flags & OFS_FLAG_RDONLY )
        stbuf->st_mode |= S_IRUSR;
    else
        stbuf->st_mode |= S_IXUSR | S_IWUSR | S_IRUSR | S_IRGRP | S_IWGRP | S_IROTH;


    if ( ! fileOpen )
        ofsCloseFile(ofs, file);

    return errcode;

cleanup:

    if ( (! fileOpen) && file )
        ofsCloseFile(ofs, file);

    return -errcode;
}


// Opero come se RENAME_NOREPLACE fosse sempre impostata
int ofs_rename (const char * src, const char * dst, unsigned int flags) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs = fc->private_data;
    OFSDir_t    * srcDir    = NULL;
    OFSDir_t    * dstDir    = NULL;
    OFSDentry_t * oldDent   = NULL;
    OFSDentry_t * newDent   = NULL;
    char * oldName          = NULL;
    char * newName          = NULL;
    char * oldPath          = NULL;
    char * newPath          = NULL;
    int errcode = 0;


    dstDir = ofsGetPathFile(ofs, dst);
    if ( dstDir != NULL )
        cleanup_errno(EEXIST);

    oldName = &src[ strlen(src) - 1 ];
    newName = &dst[ strlen(dst) - 1 ];

    for ( ; *oldName != '/' ; oldName-- );
    for ( ; *newName != '/' ; newName-- );
        
    oldName++;
    newName++;

    if (! strcmp( oldName, "." ) ||
        ! strcmp( newName, "." ) ||
        ! strcmp( oldName, ".." ) ||
        ! strcmp( newName, ".." )
        )
        cleanup_errno(EINVAL);

    if ( strlen( oldName ) > OFS_FILENAME_SAMPLE_SIZE )
        cleanup_errno(ENAMETOOLONG);

    oldPath = strndup( src,  ((void *)oldName) - ((void *)src) );
    newPath = strndup( dst,  ((void *)newName) - ((void *)dst) );

    srcDir = ofsGetPathFile(ofs, oldPath);
    dstDir = ofsGetPathFile(ofs, newPath);

    if ( ! ( srcDir && dstDir ) )
        cleanup_errno(ENOENT);

    if ( ! ( srcDir->super.flags & OFS_FLAG_DIR ) )
        cleanup_errno(ENOTDIR);

    if ( ! ( dstDir->super.flags & OFS_FLAG_DIR ) )
        cleanup_errno(ENOTDIR);

    if ( srcDir->super.flags & OFS_FLAG_RDONLY )
        cleanup_errno(EPERM);

    if ( dstDir->super.flags & OFS_FLAG_RDONLY )
        cleanup_errno(EPERM);

    oldDent = ofsGetDentry(ofs, srcDir, oldName, strlen(oldName));
    if ( ! oldDent )
        cleanup_errno(ENOENT);

    memcpy( oldDent->file_name, newName, strlen(newName) );
    oldDent->file_name_size = strlen( newName );

    if ( ofsInsertDentry(ofs, dstDir, oldDent) )
        cleanup_errno(ENOSPC);

    ofsDeleteDentry(ofs, srcDir, oldName, strlen(oldName));

    ofsFreeDentry(oldDent);
    ofsCloseFile(ofs, srcDir);
    ofsCloseFile(ofs, dstDir);
    free(oldPath);
    free(newPath);

    return 0;

cleanup:

    if ( dstDir )
        ofsCloseFile(ofs, dstDir );

    if ( srcDir )
        ofsCloseFile(ofs, srcDir);

    if ( oldPath )
        free(oldPath);

    if ( newPath )
        free(newPath);

    return -errcode;
}

int ofs_statfs (const char * path, struct statvfs * stat) {
    
    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs         = fc->private_data;
    OFSFile_t * file    = NULL;
    int errcode = 0;

    file = ofsGetPathFile(ofs, path);
    if ( ! file )
        cleanup_errno(ENOENT);

    stat->f_bsize   = ofs->boot->sec_size;
    stat->f_frsize  = ofs->boot->sec_size;
    stat->f_blocks  = ofs->boot->sec_cnt;
    stat->f_bfree   = ofs->boot->free_cls_cnt * ofs->boot->cls_size;
    stat->f_bavail  = stat->f_bfree;
    stat->f_files   = ofs->boot->cls_cnt;
    stat->f_ffree   = ofs->boot->free_cls_cnt;
    stat->f_favail  = stat->f_ffree;
    stat->f_namemax = OFS_FILENAME_SAMPLE_SIZE;

    ofsCloseFile(ofs, file);

    return 0;

cleanup:
    return -errcode;

}


