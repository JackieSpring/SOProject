
#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <sys/statvfs.h>



struct deviceStruct {
    dev_t   dev;
    uid_t   uid;
    gid_t   gid;
    mode_t  mode;
    off_t   size;
    blksize_t   blksize;
    blkcnt_t    blkcnt;
    int dd;
};
typedef struct deviceStruct DEVICE ;

/**
 * Questa funzione apre un "device" che può tanto essere un 
 * device del sistema operativo, quanto un file; se il device
 * non dovesse esistere allora creerà un file permanente sul
 * disco
 * 
 * @param   path    null-terminated string
 * @return          DEVICE handle pointer or NULL in caso di errori
*/
DEVICE * openDev( char * path );

/**
 *  Libera il device 
 *  
 * @param   dev     Un device handler
*/
void closeDev( DEVICE * dev );

/**
 *  Legge 'size` byte dal device specificato a partire dall'offset
 *  'off` e li salva in 'buf`.
 *  Non modifica il puntatore nel file impostato tramite seekDev
 *
 *  @param  buf     buffer dove salvare i dati
 *  @param  size    numero di byte da leggere
 *  @param  off     offset da cui iniziare a leggere
 *  @return         Ritorna il numero di byte letti o -1 in caso
 *                  di fallimento, se il device è stato chiuso ritorna 0
 */
ssize_t readDev( DEVICE * dev, char * buf, size_t size, off_t );
/**
 *  Scrive 'size` byte, contenuti in 'buf`, nel device specificato a 
 *  partire dall'offset 'off`.
 *  Non modifica il puntatore nel file impostato tramite seekDev
 *
 *  @param  buf     buffer di dati
 *  @param  size    numero di byte da scrivere
 *  @param  off     offset da cui iniziare a scrivere
 *  @return         Ritorna il numero di byte scritti o -1 in caso
 *                  di fallimento, se il device è stato chiuso ritorna 0
 */
ssize_t writeDev( DEVICE * dev, const char * buf, size_t size, off_t );
/**
 *  Imposta il puntatore del device all'offset specificato, il
 *  funzionamento è analogo alla syscall lseek di cui è wrapper.
 *
 *  @param  off     numero di byte di cui spostare il puntatore
 *  @param  flag    flag per specificare il comportamento della
 *                  funzione
 *  @return         Ritorna la posizione del puntatore dall'inizio
 *                  del file
*/
off_t seekDev( DEVICE * dev, off_t off, int flag );


