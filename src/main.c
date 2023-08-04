
#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <sys/errno.h>


#include "utils/generic.h"
#include "utils/devicehndl.h"
#include "utils/logger.h"
#include "ofs/ofsStructures.h"
#include "ofs/ofsModel.h"
#include "ofs/ofsListType.h"
#include "hooks/directories.h"
#include "hooks/common.h"


#define DEV_DEFAULT "/tmp/sdo0"

#define OPTION(t, p)    { t, offsetof(struct options, p), 1 }


DEVICE * dev;


static struct options {
    const char * device;
    const char * logfile;
    bool help;
} options;


static const struct fuse_opt option_spec[] = {
	OPTION("-dev %s", device),
    OPTION("-log %s", logfile),
	OPTION("-dev=%s", device),
    OPTION("-log=%s", logfile),
	OPTION("-h", help),
	OPTION("--help", help),
	FUSE_OPT_END
};

/*
getattr     // attributi dei file e directory, necessaria

create      // crea file
unlink      // disrugge file
open        // apre file
release     // sarebbe close()
read        
write
lseek

mkdir       // crea dir
rmdir       // distrugge dir
opendir     // apre directory
releasedir  // close()
readdir     // esplora dir

rename      // rinomina file

*/


//static void * of_init()

static void of_destroy( void * private_data ) {
    OFS_t * ofs = private_data;
    ofsClose(ofs);
}


static int of_open( const char * path, struct fuse_file_info * fi ) {
    return 0;
}

static int of_read( const char * path, char * buff, size_t size, off_t offset, struct fuse_file_info * fi ) {
    return 0;
}

static int of_write( const char * path, char * buf, size_t size, off_t offset, struct fuse_file_info * fi ) {
    return 0;
}



static void show_help(const char * progname) {
	printf("usage: %s [options] <mountpoint>\n\n", progname);
	printf("File-system specific options:\n"
	       "    -d <s>              Name of the device file\n"
	       "                        (default: \""DEV_DEFAULT"\")\n"
	       "    -l <s>              Name of the log file\n"
	       "                        (default no log file used))\n"
	       "\n");
}



static const struct fuse_operations of_ops = {
    .init       = NULL,//of_init,
    .destroy    = of_destroy,

    .getattr    = ofs_getattr,
    .access     = ofs_access,

    .opendir    = ofs_opendir,
    .releasedir = ofs_releasedir,

    .readdir    = ofs_readdir,
    .mkdir      = ofs_mkdir,
    .rmdir      = ofs_rmdir,
};


int main(int argc , char * argv[]) {

    int ret ;
    struct fuse_args args = FUSE_ARGS_INIT( argc, argv );



    // Interprete argomenti

    options.device = strdup(DEV_DEFAULT);
    options.logfile = NULL;
    options.help = 0;

    if ( fuse_opt_parse( &args, &options, option_spec, NULL ) < 0 )
        return 1;

    if ( options.help ) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}






    // Apertura del logger

    FILE * logfile = NULL;
    if ( options.logfile )
        logfile = fopen( options.logfile, "a" );
    
    logger = newLogger(logfile);

    fflush(stdout);


    // Apertura device

    if ( access( options.device, F_OK ) ) { // Se non esiste

        struct stat s;
        int ret;

        if( (ret = open(options.device, O_CREAT ) ) < 0 )    // crea
            cleanup_escape( logger, "Something went wrong while trying to open the device" );

        close(ret);
        
        if ( stat(options.device, &s) )         // misura
            cleanup_escape( logger, "Something went wrong while trying to open the device" );

        if ( s.st_size < MiB(25) ) 
            if ( truncate(options.device, MiB(25)) )    // ridimensiona
                cleanup_escape( logger, "Something went wrong while trying to open the device" );
    }

    dev = openDev( options.device );
    if ( dev == NULL ) {
        if ( errno == EACCES )
            cleanup_escape( logger, "Device access not allowed" );
        else 
            cleanup_escape( logger, "Something went wrong while trying to open the device" );
    }

    
    // Formattazione

    if ( ofsIsDeviceFormatted(dev) )
        notifyMessage(logger, "Il device è gia stato formattato");
    else {
        notifyMessage( logger, "Il device deve essere formattao" );

        if ( ofsFormatDevice(dev) )
            notifyError( logger, "Errore durante la formattazione del device");
    }
    printf( "dev st_blksize: %d\n", dev->dstat.st_blksize );


    OFS_t * ofs = ofsOpen( dev );
    if ( ! ofs ){
        perror("OFS non è stato aperto");
        notifyError(logger, "errore durante l'apertura del file system");
        goto cleanup;
    }

    printf( "root idx: %lu\n", ofs->root_dir->fhmem_idx );

    OFSDentry_t d = {
        .file_name = 0x6f,
        .file_name_size = 1,
        .file_first_cls = 0x19,
        .file_flags = OFS_FLAG_DIR,
    };
    OFSFile_t * f = ofsOpenFile(ofs, &d);

    printf("f idx: %lu\n", f->fhmem_idx);

    ofsCloseFile(ofs, f);

    printf( "ofs.btm: %lu\n", ofs->fhmem_bottom );
    printf( "ofs.last: %lu\n", ofs->fhmem_free);

    // DEBUGGGGGG

    //return 0;

    // DEBUGGGGG

	ret = fuse_main(args.argc, args.argv, &of_ops, ofs);
	fuse_opt_free_args(&args);
    //closeLogger(log);
	return ret;

cleanup:
	fuse_opt_free_args(&args);

    perror( "program exited with failure" );

    return 1;
}

