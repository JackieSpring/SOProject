
#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "devicehndl.h"


struct OFSBootStruct;
typedef struct OFSBootStruct OFSBoot;

struct OFSDentryStruct;
typedef struct OFSDentryStruct OFSDentry;


int ofsFormatDevice( DEVICE * dev );



