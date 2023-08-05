

#pragma once

#include <stdlib.h>
#include <stdint.h>

#define NUMHT_INIT_TABLE_SIZE 0xff

typedef uint64_t NumHTKey_t;


typedef struct NumHTEntryStruct_t {
    struct NumHTEntryStruct_t * next;
    void * value;
    NumHTKey_t key;
} NumHTEntry_t;


typedef struct NumHTStruct_t {
    size_t  size;
    size_t  table_size;
    NumHTEntry_t ** table;
} NumHT_t;


NumHT_t * numHTCreate( );
void numHTDestroy( NumHT_t *  );

void * numHTGet( NumHT_t * ht, NumHTKey_t key );
int numHTInsert( NumHT_t * ht, NumHTKey_t key, void * value );
int numHTRemove( NumHT_t * ht, NumHTKey_t key );




