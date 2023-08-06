
#include "logger.h"
#include <stdio.h>

#define LOG_MSG_STR "( MSG ) "
#define LOG_ERR_STR "[ ERR ] "



Logger * newLogger( char * logfile ) {

    FILE * fp = NULL;

    if ( logfile == NULL )
        fp = stdout;
    else
        fp = fopen( logfile, "a" );

    if ( ! fp )
        return NULL;

    Logger * ret = malloc( sizeof( Logger ) );
    if ( ret == NULL )
        return NULL;

    ret->file = fp;

    return ret;
}

void closeLogger( Logger * logger ) {
    fclose( logger->file );
    free( logger );
}


int _notify( Logger * log, char * prefix, char * fmt, va_list args ) {

    size_t full_len = strlen(fmt) + strlen( prefix ) + 2;
    char full_fmt [ full_len ];

    memset( full_fmt, 0, full_len );
    memcpy( full_fmt, prefix, strlen(prefix) + 1 );
    memcpy( full_fmt + strlen(prefix), fmt, strlen(fmt) + 1 );
    full_fmt[ full_len - 2 ] = '\n';

    int ret = vfprintf( log->file, full_fmt, args );
    fflush(log->file);

    return ret;
}


int notifyMessage( Logger * log,  char * fmt, ... ) {
    va_list args;
    va_start( args, fmt );
    int ret = _notify(log, LOG_MSG_STR, fmt, args);
    va_end( args );
}
int notifyError( Logger * log, char * fmt, ... ){
    va_list args;
    va_start( args, fmt );
    int ret = _notify(log, LOG_ERR_STR, fmt, args);
    va_end( args );
}

