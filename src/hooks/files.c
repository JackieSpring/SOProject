

#include "files.h"
#include <stdint.h>
#include <sys/fcntl.h>



int ofs_create (const char * path, mode_t mode, struct fuse_file_info * fi ) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs             = fc->private_data;
    OFSFile_t * dir         = NULL;
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fh    = NULL;
    char * filename         = NULL;
    OFSDentry_t  * dent     = NULL;
    int errcode = 0;
    OFSDentry_t * tool;


    // sufficiente spazio?
    if ( ofs->boot->free_cls_cnt < 1 )
        cleanup_errno(ENOSPC);

    dir = ofsGetParent(ofs, path);
    if ( ! dir )
        cleanup_errno(ENOENT);

    // è directory?
    if ( ! (dir->flags & OFS_FLAG_DIR) )
        cleanup_errno(EISDIR);

    // permessi d'accesso?
    if ( dir->flags & OFS_FLAG_RDONLY )
        cleanup_errno(EACCES);

    filename = &path[ strlen(path) -1 ];
    for ( ; *filename != '/' ; filename-- );
    filename++;


    // il file esiste?
    if ( ( tool = ofsGetDentry(ofs, dir, filename, strlen(filename)) ) ){
        ofsFreeDentry(tool);
        cleanup_errno(EEXIST);
    }

    // mode è valido ?
    if ( ! mode )
        cleanup_errno(EINVAL);


    dent = ofsCreateEmptyFile(ofs, filename, strlen(filename), OFS_FLAG_FILE);
    if ( ! dent )
        cleanup_errno(ENOSPC);
    
    if ( ! (mode & 0x333) )
        dent->file_flags |= OFS_FLAG_RDONLY;


    if (    ofsInsertDentry(ofs, dir, dent) )          cleanup_errno(EIO);
    if ( ! ( file = ofsOpenFile(ofs, dent) ) )      cleanup_errno(EIO);
    if ( ! ( fh = ofsOpenFileHandle(ofs, file) ) )  cleanup_errno(EIO);

    fh->open_flags = fi->flags;
    fi->fh = fh->fhmem_idx;

    ofsCloseFile(ofs, dir);

    return 0;

cleanup:

    if ( dir )
        ofsCloseFile(ofs, dir);

    if ( dent ) {
        ofsDeallocClusters(ofs, dent->file_first_cls);
        ofsFreeDentry(dent);
    }

    return -errcode;
}

int ofs_unlink (const char * path) {
    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs             = fc->private_data;
    OFSFile_t * dir         = NULL;
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fh    = NULL;
    char * filename         = NULL;
    OFSDentry_t  * dent     = NULL;
    int errcode = 0;
    OFSDentry_t * tool;


    dir = ofsGetParent(ofs, path);
    if ( ! dir )                            cleanup_errno(ENOENT); // esiste?
    if ( ! (dir->flags & OFS_FLAG_DIR) )    cleanup_errno(EISDIR); // è directory?
    if (    dir->flags & OFS_FLAG_RDONLY )  cleanup_errno(EACCES); // permessi d'accesso?

    filename = &path[ strlen(path) - 1 ];
    for ( ; *filename != '/' ; filename-- );
    filename++;

    if ( strlen(filename) > OFS_FILENAME_SAMPLE_SIZE ) cleanup_errno(ENAMETOOLONG); // nome troppo lungo?


    dent = ofsGetDentry(ofs, dir, filename, strlen(filename));
    if ( ! dent )                   cleanup_errno(ENOENT);

    file = ofsGetFile( ofs, dent->file_first_cls );
    if ( file && file->refs > 0 )   cleanup_errno(EBUSY);

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
    if ( ! file )                           cleanup_errno(ENOENT); // esiste?
    if ( file->flags & OFS_FLAG_DIR )       cleanup_errno(EISDIR); // è un file?
    if ( (flagReq & ( O_RDWR | O_WRONLY )) 
        && (file->flags & OFS_FLAG_RDONLY)) cleanup_errno(EACCES); // l'accesso è permesso?


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


int ofs_read (const char * path, char * buf, size_t size, off_t off, struct fuse_file_info * fi) {

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs             = fc->private_data;
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fh    = NULL;
    OFSCluster_t * cls      = NULL;
    off_t    cls_idx        = 0;    // indice del cluster da leggere
    uint64_t cls_off        = 0;    // offset all'interno di un cluster
    size_t read_bytes       = 0;    // byte letti fin'ora
    size_t sample_bytes     = 0;    // byte da leggere in una lettura da cluster
    off_t  sample_off       = 0;    // offset per una lettura da cluster
    int errcode = 0;


    fh = ofsGetFileHandle(ofs, fi->fh);
    if ( ! fh )
        cleanup_errno(EBADF);

    file = ofsGetFile(ofs, fh->fomem_idx);
    if ( ! file )                       cleanup_errno(EIO);     // eisste?
    if ( file->flags & OFS_FLAG_DIR )   cleanup_errno(EISDIR);  // è un file?
    if ( off > file->size )             cleanup_errno(EINVAL);  // l'offset entro limiti?
    if ( fh->open_flags & O_WRONLY )    cleanup_errno(EBADF);   // permessi d'accesso?

    if ( off + size > file->size )
        size = file->size - off;

    sample_off = off;

    while( read_bytes < size ) {
        cls_off = sample_off % ofs->cls_bytes ;
        cls_idx = sample_off / ofs->cls_bytes ;

        

        cls = ofsGetCluster(ofs, getItem(file->cls_list, cls_idx));
        if ( ! cls )
            cleanup_errno(EIO);

        sample_bytes = cls->size - cls_off ;
        if ( sample_bytes > ( size - read_bytes ) )
            sample_bytes = ( size - read_bytes );

        memcpy(buf + read_bytes, cls->data + cls_off, sample_bytes);

        read_bytes += sample_bytes;
        sample_off += sample_bytes;

        ofsFreeCluster(cls);
        
    };

    fh->seek_ptr += read_bytes;

    return read_bytes;

cleanup:
    return -errcode;
}

int ofs_write (const char * path, const char * buf, size_t size, off_t off, struct fuse_file_info * fi) {
    
    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs             = fc->private_data;
    OFSFile_t * dir         = NULL;
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fh    = NULL;
    OFSCluster_t * cls      = NULL;
    OFSDentry_t * dent      = NULL;
    off_t    cls_idx        = 0;    // indice del cluster da scrivere
    uint64_t cls_off        = 0;    // offset all'interno di un cluster
    size_t write_bytes      = 0;   // byte scritti fin'ora
    size_t sample_bytes     = 0;    // byte da scrivere in una scrittura su cluster
    off_t  sample_off       = 0;    // offset per una scrittura su cluster
    char * filename         = NULL;
    int errcode = 0;

    fh = ofsGetFileHandle(ofs, fi->fh);
    if ( ! fh )
        cleanup_errno(EBADF);

    file = ofsGetFile(ofs, fh->fomem_idx);
    if ( ! file )                       cleanup_errno(EIO);
    if ( file->flags & OFS_FLAG_DIR )   cleanup_errno(EISDIR);
    if ( off > file->size )             cleanup_errno(EINVAL);
    if ( fh->open_flags & O_RDONLY )    cleanup_errno(EBADF);


    dir = ofsGetParent(ofs, path);
    if ( ! dir )
        cleanup_errno(ECONNRESET);

    dent = ofsGetDentry(ofs, dir, file->name, file->name_size);
    if ( ! dent )
        cleanup_errno(EFBIG);



    sample_off = off;

    while( write_bytes < size ) {
        cls_off = sample_off % ofs->cls_bytes ;
        cls_idx = sample_off / ofs->cls_bytes ;

        while ( cls_idx >= file->cls_list->size )
            ofsExtendFile(ofs, file);

        cls = ofsGetCluster(ofs, getItem(file->cls_list, cls_idx));
        if ( ! cls )
            cleanup_errno(EPIPE);

        sample_bytes = cls->size - cls_off ;
        if ( sample_bytes > ( size - write_bytes ) )
            sample_bytes = ( size - write_bytes );

        memcpy(cls->data + cls_off, buf + write_bytes, sample_bytes);

        write_bytes += sample_bytes;
        sample_off += sample_bytes;

        ofsFreeCluster(cls);
        
    }

    if ( (off + write_bytes) > file->size )
        file->size = off + write_bytes;

    dent->file_size = file->size;
    ofsDeleteDentry(ofs, dir, dent->file_name, dent->file_name_size);
    ofsInsertDentry(ofs, dir, dent);

    fh->seek_ptr += write_bytes;

    ofsCloseFile(ofs, dir);
    ofsFreeDentry(dent);

    return write_bytes;

cleanup:
    return -errcode;
}


off_t ofs_lseek (const char * path, off_t off, int whence, struct fuse_file_info * fi){

    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs             = NULL;
    OFSFile_t * file        = NULL;
    OFSFileHandle_t * fh    = NULL;
    off_t   new_ptr         = 0;
    int errcode = 0;

    fh = ofsGetFileHandle(ofs, fi->fh);
    if ( ! fh )
        cleanup_errno(EBADF);

    file = ofsGetFile(ofs, fh->fomem_idx);
    if ( ! file )
        cleanup_errno(EBADF);

    if ( ! whence & ( SEEK_SET | SEEK_CUR | SEEK_END ) )
        cleanup_errno(EINVAL);

    switch(whence){
        case SEEK_SET:
            new_ptr = off;
            break;
        case SEEK_CUR:
            new_ptr = fh->seek_ptr + off;
            break;
        case SEEK_END:
            new_ptr = file->size + off;
            break;
    }

    if ( new_ptr < 0 )
        cleanup_errno(EINVAL);

    fh->seek_ptr = new_ptr;

    return fh->seek_ptr;

cleanup:
    return -errcode;

}

int ofs_truncate (const char * path, off_t size, struct fuse_file_info * fi) {
    
    struct fuse_context * fc = fuse_get_context();
    OFS_t * ofs     = fc->private_data;
    OFSDir_t * dir      = NULL;
    OFSFile_t * file    = NULL;
    OFSFileHandle_t * fh= NULL;
    OFSDentry_t * dent  = NULL;
    OFSPtr_t    cls_cnt;
    OFSPtr_t    cls;
    int errcode = 0;

    fh = ofsGetFileHandle(ofs, fi->fh);
    if ( ! fh )                                         cleanup_errno(EBADF);
    if ( ! (fh->open_flags & ( O_RDWR | O_WRONLY )) )   cleanup_errno(EINVAL);

    file = ofsGetFile(ofs, fh->fomem_idx);
    if ( ! file )                       cleanup_errno(EBADF);
    if ( file->flags & OFS_FLAG_DIR  )  cleanup_errno(EISDIR);

    dir = ofsGetParent(ofs, path);
    if ( ! dir )    cleanup_errno(EBADF);

    dent = ofsGetDentry(ofs, dir, file->name, file->name_size );
    if ( ! dent )   cleanup_errno(EBADF);

    if ( size < 0 ) cleanup_errno(EINVAL);

    cls_cnt = size / ofs->cls_bytes;
    cls_cnt++;

    if ( cls_cnt > file->cls_list->size ) {
        if ( ofsExtendFileBy(ofs, file, (cls_cnt - file->cls_list->size) ) ) cleanup_errno(EIO);
    }
    else if ( cls_cnt < file->cls_list->size )
        if ( ofsShrinkFileBy(ofs, file, ( file->cls_list->size - cls_cnt )  ) ) cleanup_errno(EIO); 

    file->size = size;
    dent->file_size = size;

    ofsDeleteDentry(ofs, dir, file->name, file->name_size);
    ofsInsertDentry(ofs, dir, dent);
    ofsFreeDentry(dent);
    ofsCloseFile( ofs, dir );

    return 0;

cleanup:
    return -errcode;
}
