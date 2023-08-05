

#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>



#define ARRAY_ITEM_INVALID -1



typedef struct ArrayStruct_t {
    off_t       bottom;
    off_t       free;
    size_t      size;
    void        ** table;
} Array_t;


Array_t * arrayCreate( size_t initSize );
void arrayDestroy( Array_t * arr );


off_t arrayInsert( Array_t * arr, void * item );
void  arrayRemove( Array_t * arr, off_t idx );
void * arrayGet( Array_t * arr, off_t idx );
