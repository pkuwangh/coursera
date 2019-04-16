#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "fatals.h"

#define BINARY_NAME "all"

void fatal(const char * msg, ...)
{
    va_list args;
    va_start(args,msg);
    vfprintf(stderr,msg,args);
    va_end(args);
    exit(1);
}

