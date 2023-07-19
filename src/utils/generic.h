
#pragma once

#include <unistd.h>

#define _FILE_OFFSET_BITS 64    // Compatibilità LFS


#ifndef bool
    #define bool int
    #define true 1
    #define false 0
#endif

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