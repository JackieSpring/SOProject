
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


int ofs_open (const char *, struct fuse_file_info *);
int ofs_release (const char *, struct fuse_file_info *);
int ofs_read (const char *, char *, size_t, off_t, struct fuse_file_info *);
int ofs_write (const char *, const char *, size_t, off_t, struct fuse_file_info *);

off_t ofs_lseek (const char *, off_t off, int whence, struct fuse_file_info *);
int ofs_truncate (const char *, off_t, struct fuse_file_info *fi);

