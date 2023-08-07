
#include "ofsStructures.h"

/*

// LO STANDARD
Il file system OilFiSh è uno standard indipendente e creato 
unicamente per questo progetto, è un file system basato su
tabella FAT di cluster di settori, pertanto non opera sul
singolo settore del device ma su un insieme di settori; 
queste caratteristiche rendono OFS sensibile a frammentazione
interna. 

// LA TABELLA FAT
La tabella fat di OFS è analoga alla tabella che si può
trovare nel file system FAT32, utilizza indici a 32 bit
per identificare i cluster e riserva alcuni cluster a scopo
di funzionamento:
    - Boot Cluster
        Il primo cluster del device è riservato alla
        struttura di boot.
    - FAT Cluster
        Il secondo cluster del device contiene la tabella
        fat stessa
    - Reserved Clusters
        I due cluster successivi alla tabella sarebbero
        liberi per l'utilizzo, ma vengono riservati
    - Special Clusters
        i Cluster ( -2, -1, 0 ) sono indici speciali usati
        a scopi di funzionamento del file system.

Dunque quattro cluster sono usati dal file system mentre i
restanti son cluster dati. La tabella FAT indicizza i cluster
partendo dal settore specificato nella struttura Boot, e non 
dall'inizio del device.
La FAT si presenta come una linked list di Cluster dove ogni
file è rappresentato da una lista collegata di cluster terminati
da OFS_LAST_CLUSTER, che in questa implementazione è il cluster 0;
i cluster liberi sono a loro volta collegati tra loro affinché
l'allocazione / deallocazione di nuovi cluster sia rapida.

// LA STRUTTURA BOOT
La struttura boot racchiude tutte le informazioni sul device
in uso e sul suo partizionamento necessarie al funzionamento
del file system, a partire dalla dimensione e numero dei settori
e dei cluster di settori, la posizione della tabella FAT e
l'indice di inizio dei cluster allocati e della root directory.
Per identificare la struttura viene usato un magic number di tre
byte ( i caratteri ascii "OFS" ), se questo codice non dovesse
essere trovato allora il device non è stato formattato secondo
lo standard OilFiSh ed è inutilizzabile allo stato attuale.
I cluster dati incominciano a partire dal settore indicato nel
campo "first_sec", utilizzato per tutte le operazioni di 
indirizzamento dei cluster.

// I FILE
Come detto in precedenza i file sono rappresentati nella fat
come catene di cluster dati, i cluster che li compongono non
contengono informazioni sul file, ma solo i suoi dati; per
ottenere le specifiche sul file sono necessarie le dentry
che li referenziano.
I file utilizzabili possono essere di tre tipi: regolari 
directory, invalidi; oltre al tipo i file possono essere
readonly o nascosti, se il file è specificato come 
nascosto allora non deve comparire nella lettura della directory.
I file sono identificati da un nome che in questa implementazione
può avere lunghezza massima di 18 caratteri.


// LA STRUTTURA DENTRY E LE DIRECTORY
Dal punto di vista del partizionamento del device e della FAT,
in OFS non c'è differenza tra file e directory se non nel loro
contenuto: le directory sono file che devono essere interpretati
come un array di Dentry. Le Dentry sono una struttura di 32 byte
contenente i dati del file referenziato e l'indice del suo primo
cluster; questi dati includono le flag, la dimensione e il nome
del file che lo identifica all'interno di una directory. Una
dentry è considerata "libera", cioè sovrascrivibile, se le flag
sono azzerate ( cioè uguali a OFS_FLAG_FREE ).
In OFS non esiste un sistema di record degli accessi ai file, ne
di possesso di un file, l'unica limitazione è la flag READONLY che
impedisce la scrittura su un file (o directory).

*/




static inline OFSClsSize_t ofsCalculateClusterSize( OFSBoot_t * ofsb ) {
    return 2;   // TODO migliorare il calcolo
}

static inline OFSPtr_t ofsCalculateClusterCount( OFSBoot_t * ofsb ) {
    OFSPtr_t cls_cnt = (( ofsb->sec_cnt ) / ofsb->cls_size ) - 1 ; // alloco un cluster per boot
    cls_cnt -= 2; // alloco due cluster per la fat table (MIGLIORARE)
    if ( cls_cnt > (OFSPtr_t)OFS_MAX_CLUSTER )
        cls_cnt = (OFSPtr_t)OFS_MAX_CLUSTER;
    
    return (uint64_t) cls_cnt ;
}

static inline OFSSec_t ofsCalculateFatStart ( OFSBoot_t * ofsb ) {
    return ofsb->cls_size;  // riserva un cluster per la boot structure
}

static inline OFSSec_t ofsCalculateFirstSec( OFSBoot_t * ofsb ) {

    register uint64_t bytes = ofsb->cls_cnt * sizeof( OFSPtr_t ) ;
    register uint64_t sec_bytes = ofsb->sec_size * ofsb->cls_size ;

    if ( bytes % sec_bytes )
        bytes += sec_bytes - (bytes % sec_bytes);
    
    return ((bytes / sec_bytes) * ofsb->cls_size) + ofsb->fat_sec ;
}


extern inline OFSSec_t ofsClusterToSector( OFSBoot_t * ofsb, OFSPtr_t cp ) {
    return ofsb->first_sec + ( cp * ofsb->cls_size );
}

extern inline OFSPtr_t ofsSectorToCluster( OFSBoot_t * ofsb, OFSSec_t sp ){
    return ( sp - ofsb->first_sec ) / ( ofsb->cls_size ) ;
}

extern inline off_t ofsSectorToByte( OFSBoot_t * ofsb, OFSSec_t sp ) {
    return sp * ofsb->sec_size ;
}




int ofsFormatDevice( DEVICE * dev ) {

    OFSBoot_t boot;
    OFSDentry_t rootd;
    int ret = 0;

    boot.magic = OFS_MAGIC;
    boot.version = OFS_VERSION;
    boot.sec_size = dev->blksize;
    boot.sec_cnt = dev->blkcnt;
    boot.root_dir_ptr = OFS_INVALID_CLUSTER;  // DA CALCOLARE
    boot.free_cls_cnt = 0;  // DA CALCOLARE
    boot.free_ptr = 0;      // DA CALCOLARE


    boot.cls_size = ofsCalculateClusterSize( &boot );
    boot.cls_cnt = ofsCalculateClusterCount( &boot );

    boot.fat_sec = ofsCalculateFatStart( &boot );
    boot.first_sec = ofsCalculateFirstSec( &boot );


    // Formatta la fat scrivendola un cluster per volta
    // le prime due entry della fat sono cluster riservati,
    // il file system incomincia dal cluster [2]
    // tutte le entry non corrispondenti a cluster reali
    // (ovvero quelle oltre la tabella) sono marchiate come
    // INVALID
    // Inizializza root directory (vuota) e linked clist di
    // cluster liberi, inoltre inizializza i relativi campi
    // nel boot sector 

    OFSPtr_t fatPages = ( boot.first_sec - boot.fat_sec ) / boot.cls_size ;
    uint64_t fatPageSize = boot.cls_size * boot.sec_size ;
    uint64_t fatPageEntries = fatPageSize / sizeof( OFSPtr_t ) ;

    OFSPtr_t * fat_page = ( OFSPtr_t * ) malloc( fatPageSize ) ;
    OFSPtr_t counter = 0;

    for ( OFSPtr_t page = 0 ; page < fatPages; page++ ) {

        for ( uint64_t i = 0; i < fatPageEntries; i++ ){
            fat_page[i] = counter + 1;
            counter++;
        }

        if ( page == (fatPages - 1) ) {
            register uint64_t ovf = counter - boot.cls_cnt ;
            memset( &fat_page[ fatPageEntries - ovf ], (OFSPtr_t) OFS_INVALID_CLUSTER, ovf * sizeof(OFSPtr_t) );
            fat_page[ fatPageEntries - ovf - 1 ] = OFS_LAST_CLUSTER;
        }

        // Inizializza cluster riservati, cluster liberi e root directory
        if ( page == 0 ) {
            for ( uint64_t i = 0; i < OFS_FIRST_DATA_CLUSTER ; i++ )
                fat_page[i] = (OFSPtr_t) OFS_RESERVED_CLUSTER;
            
            //fat_page[ OFS_FIRST_DATA_CLUSTER ] = OFS_LAST_CLUSTER;
            //boot.root_dir_ptr = OFS_FIRST_DATA_CLUSTER;

            boot.free_ptr = OFS_FIRST_DATA_CLUSTER;
            boot.free_cls_cnt = boot.cls_cnt - OFS_FIRST_DATA_CLUSTER;

        }

        ret = writeDev( dev, fat_page, fatPageSize, (boot.fat_sec * boot.sec_size) + (fatPageSize * page ) );
        if ( ret < 1 )
            goto cleanup;
    }


    // Boot 
    ret = writeDev( dev, &boot, sizeof( OFSBoot_t ), 0 );
    if ( ret < 1 )
        goto cleanup;


    return 0;

cleanup:

    if ( fat_page )
        free(fat_page);

    return -1;
    
}

bool ofsIsDeviceFormatted( DEVICE * dev ) {
    uint32_t magic = 0;

    if ( readDev( dev, &magic, 3, 0 ) < 3 )
        return false;

    if ( magic - OFS_MAGIC )
        return false;

    return true;
    
}