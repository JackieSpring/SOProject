
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "../utils/generic.h"
#include "../utils/devicehndl.h"
#include "../utils/numhashtable.h"
#include "../utils/array.h"
#include "ofsModel.h"
#include "ofsStructures.h"
#include "ofsListType.h"




/**
 *  Firma per le funzioni di callback passate a ofsDirectoryIterator
 *  
 *  @param  dentry  Puntatore a uno slot dentry, la dentry potrebbere
 *                  essere vuota
 *  @param  extra   puntatore passato attraverso l'argomento 'extra' in ofsDirectoryIterator
 *
 *  @return     il valore di ritorno controlla l'iterazione attraverso la directory
 *              true -> continua l'iterazione   false -> interrompila
*/
typedef bool (* directory_iterator) ( OFSDentry_t * dentry, void * extra );



/**
 *  Inserisce una dentry in una directory; la funzione non gestisce
 *  i casi di duplicati. La nuova dentry sarà una copia della dentry 
 *  passata tra gli argomenti.
 *
 *  @param  ofs 
 *  @param  file    directory in cui inserire la dentry
 *  @param  dent    dentry da inserire
 *
 *  @return     0 o -1 in caso di errore
 *
*/
int ofsInsertDentry( OFS_t * ofs, OFSFile_t * file, OFSDentry_t * dent );
void ofsDeleteDentry( OFS_t * ofs, OFSFile_t * dir, const char * fname, size_t fnsize );


/**
 *  Itera attraverso tutti dentry slot di una directory richiamando per
 *  ciascuna la funzione di callback.
 *
 *  @param  dir     la directory da eplorare
 *  @param  callback funzione di callback richiamata per ciascuno slot
 *  @param  extra   puntatore facoltativo che sarà passato come argomento
 *                  alla funzione di callback
 *
 *  @return     0 iterazione terminata senza errori, -1 interruzione inaspettata
*/
int ofsDirectoryIterator( OFS_t * ofs, OFSFile_t * dir, directory_iterator callback, void * extra );



/**
 *  Ottiene un file a partire dal percorso
*/
OFSFile_t * ofsGetPathFile( OFS_t * ofs, char * path );

/**
 *  Restituisce la directory genitore del file identificato
 *  dal percorso.
 *
 *  @param  path    Percorso al file di cui si vuole ottenere
 *                  il genitore
 *
 *  @return     Puntatore o NULL in caso di errore
*/
OFSFile_t * ofsGetParent( OFS_t * ofs, const char * path );

/**
 *  Estende o riduce il numero di cluster occupati da un file
 *  se il file esteso è una directory allora i cluster verranno
 *  riempiti di null-byte.
 *  In caso di errore i cluster saranno deallocati / reallocati.
 *  
 *  @param  file    File che dovraà essere esteso / ridotto
 *  @param  cnt     Numero di cluster da allocare / deallocare
 *
 *  @return     0 successo, -1 fallimento
*/
int ofsExtendFile( OFS_t * ofs, OFSFile_t * file );
int ofsShrinkFile( OFS_t * ofs, OFSFile_t * file );
int ofsExtendFileBy( OFS_t * ofs, OFSFile_t * file, size_t cnt );
int ofsShrinkFileBy( OFS_t * ofs, OFSFile_t * file, size_t cnt );

