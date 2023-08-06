
#include "devicehndl.h"

struct DeviceStruct;


// !!!!!!!!!!!!!!!!!!!!!!!!!!!
// le stat di un file non descrivono il device sottostante
// pertanto bisogna trovare un altro modo per ottenere la 
// block size

DEVICE * openDev( char * path ) {
    DEVICE * ret;
    struct stat dstat;
    struct statvfs dstatvfs;

    ret = malloc( sizeof( DEVICE ) );
    if ( ret == NULL )
        goto cleanup;

    ret->dd = open( path, O_RDWR | O_CREAT );
    if ( ret->dd < 0 )
        goto cleanup;

    if ( fstat( ret->dd, &dstat ) < 0 )
        goto cleanup;


    if ( S_ISBLK(dstat.st_mode) ) {
        ioctl( ret->dd, BLKSSZGET, &ret->blksize );
        ioctl( ret->dd, BLKGETSIZE, &ret->blkcnt );
        ret->size = ret->blkcnt * ret->blksize;
    }
    else if ( S_ISREG(dstat.st_mode) ) {
        fstatvfs( ret->dd, &dstatvfs );
        ret->blksize = dstatvfs.f_bsize;
        ret->blkcnt = dstat.st_size / ret->blksize;
        ret->size = ret->blksize * ret->blkcnt;
    }
    else
        goto cleanup;

    ret->mode = dstat.st_mode;
    ret->uid = dstat.st_uid;
    ret->gid = dstat.st_gid;
    ret->dev = dstat.st_dev;

    return ret;
    
cleanup:
    if ( ret )
        free(ret);

    return NULL;

}
void closeDev( DEVICE * dev ) {
    close( dev->dd );
    free( dev );
}


ssize_t readDev( DEVICE * dev, char * buf, size_t size, off_t off ) {
    ssize_t ret;
    ssize_t read_byte = 0;

puts("READ START");
    while ( read_byte < size ) {
        ret = pread( dev->dd, buf + read_byte, size - read_byte, off + read_byte );

        if ( ret < 0 && errno == EINTR ) continue;
        if ( ret < 0 ) return ret;
        if ( ret == 0 && size != 0 ) return 0;

        read_byte += ret;
    }
puts("READ DONE");
    return read_byte;
}

ssize_t writeDev( DEVICE * dev, const char * buf, size_t size, off_t off ) {
    ssize_t ret;
    ssize_t write_byte = 0;

    while( write_byte < size ) {
        ret = pwrite( dev->dd, buf + write_byte, size - write_byte, off + write_byte );

        if ( ret < 0 && errno == EINTR ) continue;
        if ( ret <  0 ) return ret;
        if ( ret == 0 && size != 0 ) return ret;

        write_byte += ret;
    }

    return write_byte;
}

off_t seekDev( DEVICE * dev, off_t off, int whence ) {
    return lseek( dev->dd, off, whence );
}




