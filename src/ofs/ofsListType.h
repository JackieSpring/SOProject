
#pragma once


#include "ofsStructures.h"


typedef struct OFSPtrListStruct_t {
    OFSPtr_t    head;
    OFSPtr_t    tail;
    OFSPtr_t    size;
    OFSPtr_t  * table;
} OFSPtrList_t;


//OFSPtrList_t * createList( OFSPtr_t size );
//int initList( OFSPtrList_t * list, OFSPtr_t * fat, OFSPtr_t start );
OFSPtrList_t * createList( OFSPtr_t * fat, OFSPtr_t start );
void destroyList( OFSPtrList_t * list );

OFSPtr_t getItem( OFSPtrList_t * list, off_t idx );

OFSPtr_t appendItem( OFSPtrList_t * list , OFSPtr_t item );
OFSPtr_t popItem( OFSPtrList_t * list ) ;
