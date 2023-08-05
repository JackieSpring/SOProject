

#include "numhashtable.h"



NumHT_t * numHTCreate( ) {
    NumHT_t * ret           = NULL;
    NumHTEntry_t ** table    = NULL;

    ret = calloc( 1, sizeof(NumHT_t) );
    if ( ! ret )
        goto cleanup;

    table = calloc( NUMHT_INIT_TABLE_SIZE, sizeof(NumHTEntry_t *) );
    if ( ! table )
        goto cleanup;

    ret->size       = 0;
    ret->table_size = NUMHT_INIT_TABLE_SIZE;
    ret->table      = table;

    return ret;

cleanup:

    if ( ret )
        free(ret);

    if ( table )
        free(table);

    return NULL;
}

void numHTDestroy( NumHT_t * ht ) {
    if ( ! ht )
        return;

    NumHTEntry_t * prevNode = NULL;
    NumHTEntry_t * node = NULL;

    for ( size_t i = 0; i < ht->table_size; i++ ) {
        node = ht->table[i];

        for ( ; node != NULL ; prevNode = node, node = node->next, free(prevNode) );
    }

    free( ht->table );
    free(ht);
}

void * numHTGet( NumHT_t * ht, NumHTKey_t key ) {
    if ( ! ht )
        return NULL;

    void * ret = NULL;
    NumHTKey_t tableKey;
    NumHTEntry_t * node;

    tableKey = key % ht->table_size;
    node = ht->table[tableKey];

    for ( ; node != NULL && node->key != key ; node = node->next  );

    if ( ! node )
        return NULL;

    ret = node->value;

    return ret ;
}

int numHTInsert( NumHT_t * ht, NumHTKey_t key, void * value ) {
    if ( ! ht )
        return -1;

    NumHTKey_t tableKey;
    NumHTEntry_t ** point;
    NumHTEntry_t * node;
    NumHTEntry_t * newNode = NULL;

    tableKey = key % ht->table_size;
    point = &ht->table[tableKey];
    node = *point;

    for ( ; node != NULL && node->key != key ; point = &node->next, node = node->next  );

    // Se esiste gia la key, sostituisce il valore
    if ( node ) {
        node->value = value;
        return 0;
    }

    newNode = calloc(1, sizeof(NumHTEntry_t));
    if ( ! newNode )
        goto cleanup;
    
    newNode->key = key;
    newNode->value = value;
    newNode->next = NULL;
    *point = newNode;

    ht->size++;

    return 0;

cleanup:

    if ( newNode )
        free(newNode);

    return -1;
    
}

int numHTRemove( NumHT_t * ht, NumHTKey_t key ) {
    if ( ! ht )
        return -1;

    NumHTKey_t tableKey;
    NumHTEntry_t ** point;
    NumHTEntry_t * node;
    NumHTEntry_t * oldNode = NULL;

    tableKey = key % ht->table_size;
    point = &ht->table[tableKey];
    node = *point;

    for ( ; node != NULL && node->key != key ; point = &node->next, node = node->next  );

    if ( ! node )
        return -1;

    *point = node->next;
    free(node);

    ht->size--;

    return 0;
}

