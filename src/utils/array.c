

#include "array.h"



Array_t * arrayCreate( size_t size ) {
    
    Array_t * ret   = NULL;
    void * table    = NULL;

    ret = calloc( 1, sizeof( Array_t ) );
    if ( ! ret )
        goto cleanup;

    table = calloc( size, sizeof( void * ) );
    if ( table == NULL )
        return NULL;
    
    ret->table          = table;
    ret->size           = size;
    ret->bottom         = 0;
    ret->free           = ARRAY_ITEM_INVALID;

    return ret;

cleanup:
    if ( ret )
        free(ret);

    if ( table )
        free(table);

    return NULL;
}

void arrayDestroy(Array_t *arr) {
    if ( ! arr )
        return;

    free( arr->table );
    free(arr);
}


off_t arrayInsert(Array_t *arr, void * item) {

    if ( ! arr )
        return ARRAY_ITEM_INVALID;

    off_t ret;
    off_t size;
    off_t free;
    void ** table;

    table = arr->table;
    size = arr->size;
    free = arr->free;

    // Se lo spazio è esaurito espande l'array
    if ( arr->bottom == ARRAY_ITEM_INVALID ) {
        table = realloc( arr->table, 2 * size * sizeof( void * ) );
        if ( table == NULL )
            goto cleanup;

        arr->table = table;
        arr->size = 2 * size;
        arr->bottom = size;
        size = 2 * size;
    }

    // Se sono stati deallocati dei blocchi allora usali
    if ( free != ARRAY_ITEM_INVALID ) {
        arr->free = (off_t) table[ free ];
        table[ free ] = item;
        ret = free;
    }
    else {  // altrimenti metti in fondo
        table[ arr->bottom ] = item;
        ret = arr->bottom;
        arr->bottom++;

        if ( arr->bottom == arr->size )
            arr->bottom = ARRAY_ITEM_INVALID;
    }

    return ret;

cleanup:
    return ARRAY_ITEM_INVALID;
}

void arrayRemove(Array_t *arr, off_t idx){
    if ( ! arr )
        return;

    if ( idx == ARRAY_ITEM_INVALID )
        return;

    arr->table[ idx ] = (void *)arr->free;
    arr->free = idx;

    return;
}


void * arrayGet(Array_t *arr, off_t idx) {

    if ( ! arr )
        return NULL;

    if ( idx == ARRAY_ITEM_INVALID )
        return NULL;

    return arr->table[idx];

}




