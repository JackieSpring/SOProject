
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>



struct LoggerStruct;
typedef struct LoggerStruct Logger;


// NULL for stdout
Logger * newLogger( FILE * logfile );

void closeLogger( Logger * );


int notifyMessage( Logger *,  char * fmt, ... );
int notifyError( Logger *, char * fmt, ... );

