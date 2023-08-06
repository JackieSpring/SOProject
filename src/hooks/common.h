
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
#include "../ofs/ofsUtils.h"


int ofs_access (const char * path, int req);
int ofs_getattr(const char * path, struct stat *stbuf, struct fuse_file_info *fi);
int ofs_rename (const char *, const char *, unsigned int flags);
int ofs_statfs (const char *, struct statvfs *);

// Non implementati
int ofs_utimens (const char *, const struct timespec tv[2], struct fuse_file_info *fi);
int ofs_ioctl (const char *, unsigned int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data);