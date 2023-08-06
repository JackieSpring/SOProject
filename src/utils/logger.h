
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>



struct LoggerStruct {
    FILE * file;
};
typedef struct LoggerStruct Logger;


// NULL for stdout
Logger * newLogger( char * path );

void closeLogger( Logger * );


int notifyMessage( Logger *,  char * fmt, ... );
int notifyError( Logger *, char * fmt, ... );

