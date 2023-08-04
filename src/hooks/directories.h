
#pragma once

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <sys/errno.h>

#include "../utils/generic.h"
#include "../utils/logger.h"
#include "../ofs/ofsModel.h"
#include "../ofs/ofsStructures.h"


int ofs_opendir (const char * path, struct fuse_file_info * fi);
int ofs_getattr(const char * path, struct stat *stbuf, struct fuse_file_info *fi);
int ofs_readdir(
        const char * path, 
        void * buf, 
        fuse_fill_dir_t filler, 
        off_t offset, 
        struct fuse_file_info *fi, 
        enum fuse_readdir_flags flags
    );
int ofs_mkdir (const char *, mode_t);
int ofs_rmdir(const char * path);

