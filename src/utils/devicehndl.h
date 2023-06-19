
#pragma once

#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>



struct deviceStruct {
    struct stat dstat;
    int dd;
};
typedef struct deviceStruct DEVICE ;


DEVICE * openDev( char * path );
void closeDev( DEVICE *  );

ssize_t readDev( DEVICE * dev, char * buf, size_t size, off_t );
ssize_t writeDev( DEVICE * dev, const char * buf, size_t size, off_t );
off_t seekDev( DEVICE * dev, off_t off, int flag );


void closeDev( DEVICE * dev );


