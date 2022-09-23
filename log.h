#ifndef LOG_H
#define LOG_H

#include <assert.h>
#include <stdio.h>
#include <time.h>

enum class log
{
    DBG = 1,
    INF = 2,
    WRN = 3,
    ERR = 4,
};

const size_t __TIME_BUF_SIZE = 10;

#ifndef __LOG_CPP
extern log __LOG_LEVEL;
extern FILE *__LOG_OUT_STREAM;
#endif


#define R       "\033[91m"
#define G       "\033[92m"
#define Cyan    "\033[96m"
#define Y       "\033[93m"
#define D       "\033[39m"


static inline void current_time (char *buf, size_t buf_size);

/**
 * @brief      Write to log stream time, file&line and your formatted message
 *
 * @param      lvl   Log level
 * @param      ...   printf parameters (format string & it's parameters)
 */
#ifndef DISABLE_LOGS
#define log(lvl, ...)                                                   \
{                                                                       \
    if (lvl >= __LOG_LEVEL)                                             \
    {                                                                   \
        char time_buf[__TIME_BUF_SIZE] = "";                            \
        current_time (time_buf, __TIME_BUF_SIZE);                       \
        fprintf (__LOG_OUT_STREAM, "%s ", time_buf);                    \
                                                                        \
        if      (lvl == log::DBG) { fprintf (__LOG_OUT_STREAM, "DEBUG"); }                 \
        else if (lvl == log::INF) { fprintf (__LOG_OUT_STREAM, Cyan "INFO " D); }          \
        else if (lvl == log::WRN) { fprintf (__LOG_OUT_STREAM, Y "WARN " D); }             \
        else if (lvl == log::ERR) { fprintf (__LOG_OUT_STREAM, R "ERROR" D); }             \
                                                                        \
        fprintf (__LOG_OUT_STREAM, " [" __FILE__ ":%d] ", __LINE__);    \
        fprintf (__LOG_OUT_STREAM, __VA_ARGS__);                        \
        fputc   ('\n', __LOG_OUT_STREAM);                               \
    }                                                                   \
}                                                       

#else

#define log(lvl, ...) {;}

#endif
/**
 * @brief      Sets the log level.
 *
 * @param[in]  level  The level
 */

void set_log_level (log level);

/**
 * @brief      Sets the output log stream.
 *
 * @param      stream  Stream
 */
void set_log_stream (FILE *stream);

FILE *get_log_stream ();

/**
 * @brief      Write current time in HH:MM:SS format to given buffer
 *
 * @param[out] buf       Output buffer
 * @param[in]  buf_size  Buffer size
 */
static inline void current_time (char *buf, size_t buf_size)
{
    assert (buf != nullptr && "pointer can't be null");
    assert (buf_size >= 9 && "Small buffer");

    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    strftime (buf, buf_size, "%H:%M:%S", timeinfo);
}
#endif //LOG_H