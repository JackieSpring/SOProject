

#include "ofsListType.h"



OFSPtrList_t * createList( OFSPtr_t * fat, OFSPtr_t start ) {

    if ( ! fat )
        goto cleanup;

    if (start == OFS_INVALID_CLUSTER ||
        start == OFS_RESERVED_CLUSTER ||
        start == OFS_LAST_CLUSTER )
        goto cleanup;



    OFSPtr_t size = 0;
    OFSPtrList_t * l = NULL;
    OFSPtr_t * table = NULL;
    OFSPtr_t node = start;

    for ( size = 0; node != OFS_LAST_CLUSTER ; size++ )  // ottiene la size
        node = fat[node];
    

    l = (OFSPtrList_t *) calloc( 1, sizeof(OFSPtrList_t) );
    if ( l == NULL )
        goto cleanup;

    l->head = OFS_INVALID_CLUSTER;
    l->tail = OFS_INVALID_CLUSTER;
    l->size = size;
    l->table = NULL;


    table = calloc( size, sizeof(OFSPtr_t) );
    if ( table == NULL )
        goto cleanup;

    for ( OFSPtr_t i = 0, node = start; i < l->size ; i++ ) {
        if ( node == OFS_LAST_CLUSTER )
            goto cleanup;
        
        table[i] = node ;
        node = fat[node];
    }

    if ( node != OFS_LAST_CLUSTER )
        goto cleanup;

    l->table = table;
    l->head = l->table[0];
    l->tail = l->table[ l->size - 1 ];

    return l;

cleanup:

    if (table)
        free(table);

    if ( l )
        free( l );

    return NULL;
}


OFSPtr_t getItem( OFSPtrList_t * list, off_t idx ) {
    if ( ! list )
        goto cleanup;

    if ( !(idx < list->size && idx >= 0 ) )
        goto cleanup;

    return list->table[idx];

cleanup:
    return OFS_INVALID_CLUSTER;
}


// Ritorna l'elemento precedente a quello inserito 
OFSPtr_t appendItem( OFSPtrList_t * list , OFSPtr_t item ) {
    if ( ! list )
        return OFS_INVALID_CLUSTER;

    if (item == OFS_INVALID_CLUSTER ||
        item == OFS_RESERVED_CLUSTER ||
        item == OFS_LAST_CLUSTER )
        return OFS_INVALID_CLUSTER;

    OFSPtr_t ret;
    OFSPtr_t * table;

    table = realloc( list->table, ( list->size * sizeof( OFSPtr_t ) ) + sizeof( OFSPtr_t ) );
    if ( table == NULL )
        goto cleanup;

    table[ list->size ] = item;
    ret = table[ list->size -1 ];
    list->size++;
    list->tail = item;
    list->table = table;

    if ( list->size == 1 )
        list->head = item;

    return ret;

cleanup:
    return OFS_INVALID_CLUSTER;
}


OFSPtr_t popItem( OFSPtrList_t * list ) {

    if ( ! list )
        goto cleanup;;

    if ( list->size == 0 )
        goto cleanup;

    OFSPtr_t ret;
    OFSPtr_t * table;

    list->size--;
    ret = list->table[list->size];

    if ( list->size != 0 )
        list->tail = list->table[ list->size - 1 ];
    else {
        list->head = OFS_INVALID_CLUSTER;
        list->tail = OFS_INVALID_CLUSTER;
    }

    table = realloc( list->table, ( list->size * sizeof( OFSPtr_t ) ) );
    if ( table == NULL )
        goto cleanup;

    list->table = table;

    return ret;

cleanup:

    return OFS_INVALID_CLUSTER;
}



void destroyList( OFSPtrList_t * list ) {
    if ( ! list )
        return;
    
    free( list->table );
    free( list );

}