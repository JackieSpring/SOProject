
#pragma once

#include <stdlib.h>
#include <unistd.h>



#define _FILE_OFFSET_BITS 64    // Compatibilità LFS


#ifndef bool
    #define bool int
    #define true 1
    #define false 0
#endif

#define KiB( x ) (x*1024)
#define MiB(x) ( KiB(x) * 1024 )
#define GiB(x) ( MiB(x) * 1024 )

#ifndef SYS_PAGE_SIZE
#define SYS_PAGE_SIZE sys_page_size
#define MASK_SYS_PAGE_SIZE mask_sys_page_size

static long sys_page_size = 0;
static long mask_sys_page_size = -1;

static void __attribute__((constructor)) _initGlobalVars(){
    sys_page_size = sysconf( _SC_PAGESIZE );
    mask_sys_page_size = ~(-sys_page_size);
}
#endif


#define cleanup_escape( log, msg ) do{ notifyError(log, msg); goto cleanup; }while(0)
#define cleanup_errno( err ) do{ errcode = err; goto cleanup; }while(0);


char ** tokenize( const char * str, char c );
void free_str_arr( const char ** arr );
