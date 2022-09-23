#define __LOG_CPP

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "log.h"

log __LOG_LEVEL = log::WRN;
FILE *__LOG_OUT_STREAM = stdout;

char __GLOBAL_TIME_BUF[__GLOBAL_TIME_BUF_SIZE] = "";

void set_log_level (log level)
{
    __LOG_LEVEL = level;
}

void set_log_stream (FILE *stream)
{
    assert (stream != NULL);

    __LOG_OUT_STREAM = stream;
}