
OILFISH file system


Questo programma è u driver user-space per file system OILFiSh
creato per l'esame di sistemi operativi; è compilabile solo su
sistemi linux.
Per funzionare si deve avere installato il framework Fusefs
( questo è un articolo di IBM sul suo funzionamento e installazione 
https://developer.ibm.com/articles/l-fuse/ ).

// Compilare
Nella root directory eseguire il comando 
    make
verrà creato un file chiamato 'ofs.fs'.

// Eseguire
Per eseguire il programma è suffieciente indicargli il mount-point
del file system.

    ./ofs.fs <mount-directory>

Verrà creato un file immagine /tmp/sdo0 che fungerà da device di
memorizzazione, in alternativa è possibile passare un device
di memorizzazione specifico tramite l'opzione -dev

    ./ofs.fs -dev=<my-device> <mount-directory>

Se il device non dovesse essere formattto secondo lo standard
OilFisH, il programma lo formatterà, questa operazione potrebbe
richiedere del tempo per file di grandi dimensioni.
Completate queste procedure si potrà navigare nel file system
attraverso la directory di mount.

// Unmount
Per smontare il file system si esegua

    fusermount -u <mount-directory>

// Operazioni
Queste sono le operazioni disponibili:
    access
    getattr
    statfs
    open
    close
    read
    write
    lseek
    truncate
    mkdir
    rmdir
    readddir
    opendir
    releasedir
