
#pragma once

#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>


struct deviceStruct;
typedef DEVICE struct deviceStruct;


DEVICE * openDev( char * path );

ssize_t readDev( DEVICE * dev, char * buf, size_t size );
ssize_t writeDev( DEVICE * dev, const char * buf, size_t size );
off_t seekDev( DEVICE * dev, off_t off, int flag );


void closeDev( DEVICE * dev );


