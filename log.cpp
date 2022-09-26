#define __LOG_CPP

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "log.h"

log __LOG_LEVEL = log::WRN;
FILE *__LOG_OUT_STREAM = stdout;

void set_log_level (log level)
{
    __LOG_LEVEL = level;
}

void set_log_stream (FILE *stream)
{
    assert (stream != NULL);

    __LOG_OUT_STREAM = stream;
}

FILE *get_log_stream ()
{
    return __LOG_OUT_STREAM;
}

void _log (log lvl, const char *fmt, const char *file, unsigned int line...)
{
    va_list args;
    va_start (args, line);

    if (lvl >= __LOG_LEVEL)                                             
    {                                                                   
        char time_buf[__TIME_BUF_SIZE] = "";                            
        current_time (time_buf, __TIME_BUF_SIZE);                       
        fprintf (__LOG_OUT_STREAM, "%s ", time_buf);                    
                                                                        
        if      (lvl == log::DBG) { fprintf (__LOG_OUT_STREAM, "DEBUG"); }                 
        else if (lvl == log::INF) { fprintf (__LOG_OUT_STREAM, Cyan "INFO " D); }          
        else if (lvl == log::WRN) { fprintf (__LOG_OUT_STREAM, Y "WARN " D); fflush (__LOG_OUT_STREAM); }             
        else if (lvl == log::ERR) { fprintf (__LOG_OUT_STREAM, R "ERROR" D); fflush (__LOG_OUT_STREAM); }             
                                                                        
        fprintf (__LOG_OUT_STREAM, " [%s:%u] ", file, line);    
        vfprintf (__LOG_OUT_STREAM, fmt, args);                        
        fputc   ('\n', __LOG_OUT_STREAM);                               
    }       

    va_end (args);                                                            
}