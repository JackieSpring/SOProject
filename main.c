
#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>

#include "utils/devicehndl.h"


#ifndef bool
    #define bool int
#endif

#define DEV_DEFAULT "/tmp/sdo0"

#define OPTION(t, p)    { t, offsetof(struct options, p), 1 }


DEVICE * dev;

static struct options {
    const char * device;
    const char * logfile;
    bool help;
} options;


static const struct fuse_opt option_spec[] = {
	OPTION("-d=%s", device),
    OPTION("-l=%s", logfile),
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


static void * of_init( struct fuse_conn_info * fi, struct fuse_config * cfg ) {

    struct fuse_context * fctx = fuse_get_context();

    printf( "fuse context fuse: %p uid: %d gid: %d pid: %d umask: %d\n", fctx->fuse, fctx->uid, fctx->gid, fctx->pid, fctx->umask );

    return NULL;
}

static int of_getattr(const char * path, struct stat *stbuf, struct fuse_file_info *fi) {
    memset( stbuf, 0, sizeof( struct stat ) );
    stbuf->st_mode = S_IFDIR | 777 ;
    stbuf->st_nlink = 1;
    return 0;
}

static int of_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    filler( buf, ".", NULL, 0, 0 );
    filler( buf, "..", NULL, 0, 0 );
    return 0;
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
    .init       = of_init,
    .readdir    = of_readdir,
    .getattr    = of_getattr,
};


int main(int argc , char * argv[]) {

    int ret ;
    struct fuse_args args = FUSE_ARGS_INIT( argc, argv );

    options.device = DEV_DEFAULT;
    options.logfile = NULL;

    if ( fuse_opt_parse( &args, &options, option_spec, NULL ) < 0 )
        return 1;

    if ( options.help ) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}

    // Apertura device

    if ( options.device ) {
        dev = openDev( options.device );
        if ( dev == NULL )
            return 1;
        

    }
    

	ret = fuse_main(args.argc, args.argv, &of_ops, NULL);
	fuse_opt_free_args(&args);
	return ret;
}

