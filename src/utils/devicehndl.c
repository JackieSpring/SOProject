
#include "devicehndl.h"



struct DeviceStruct;


// !!!!!!!!!!!!!!!!!!!!!!!!!!!
// le stat di un file non descrivono il device sottostante
// pertanto bisogna trovare un altro modo per ottenere la 
// block size

DEVICE * openDev( char * path ) {
    DEVICE * ret;

    ret = malloc( sizeof( DEVICE ) );
    if ( ret == NULL )
        goto cleanup;

    ret->dd = open( path, O_RDWR | O_CREAT );
    if ( ret->dd < 0 )
        goto cleanup;

    if ( fstat( ret->dd, &ret->dstat ) < 0 )
        goto cleanup;

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


    while ( read_byte < size ) {
        ret = pread( dev->dd, buf + read_byte, size - read_byte, off + read_byte );

        if ( ret < 0 && errno == EINTR ) continue;
        if ( ret < 0 ) return ret;
        if ( ret == 0 && size != 0 ) return 0;

        read_byte += ret;
    }

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




