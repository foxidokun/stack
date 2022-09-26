#ifndef STACK_H
#define STACK_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "log.h"

typedef unsigned char err_flags;
typedef void (*elem_print_f) (void *elem, size_t elem_size, FILE *stream);

enum res
{
    OK              = 0,
    NULLPTR         = 1<<0,
    OVER_FILLED     = 1<<1,
    POISONED        = 1<<2,
    NOMEM           = 1<<3,
    EMPTY           = 1<<4,
    BAD_CAPACITY    = 1<<5,
    CORRUPTED       = 1<<6
};

#ifndef NDEBUG
struct stack_debug_t
{
    const char *const func_name;
    const char *const file;
    const char *const var_name;
    unsigned int line;
};
#endif

struct stack_t
{
    void *data;
    size_t size;
    size_t capacity;
    size_t obj_size;
    size_t reserved;
    elem_print_f print_func;

    #ifndef NDEBUG
    const stack_debug_t *debug_data;
    #endif
};

err_flags __stack_ctor (stack_t *stk, size_t obj_size, size_t capacity = 0, elem_print_f print_func = nullptr);

#ifndef NDEBUG
err_flags __stack_ctor_with_debug (stack_t *stk, const stack_debug_t *debug_data,
                                size_t obj_size, size_t capacity = 0, elem_print_f print_func = nullptr);
#endif

#ifndef NDEBUG

    #define stack_ctor(stk, obj_size, ...)                                          \
    {                                                                               \
        const static stack_debug_t debug_info = {__PRETTY_FUNCTION__, __FILE__,     \
                                            #stk, __LINE__};                        \
        __stack_ctor_with_debug (stk, &debug_info, obj_size, ##__VA_ARGS__);        \
    }

#else

    #define stack_ctor(stk, obj_size, ...)                  \
    {                                                        \
        __stack_ctor(stk, obj_size, ##__VA_ARGS__);           \
    }

#endif

err_flags stack_resize (stack_t *stk, size_t new_capacity);

err_flags stack_shrink_to_fit (stack_t *stk);

err_flags stack_push (stack_t *stk, const void *value);

err_flags stack_pop (stack_t *stk, void *value);

err_flags stack_dtor (stack_t *stk);

void stack_dump (const stack_t *stk, FILE *stream);

void stack_perror (err_flags errors, FILE *stream, const char *prefix = nullptr);

err_flags stack_verify (const stack_t *stk);

err_flags data_poison_check (const stack_t *stk);

void byte_fprintf(void *elem, size_t elem_size, FILE *stream);

#ifndef NDEBUG

    #define stack_assert(stk)                                   \
    {                                                           \
        err_flags check_res = stack_verify(stk);                \
        if (check_res != res::OK)                               \
        {                                                       \
            log(log::ERR,                                       \
                "Failed stack check with err flags: ");         \
            stack_perror (check_res, get_log_stream(), "->");   \
            stack_dump(stk, get_log_stream());                  \
            return check_res;                                   \
        }                                                       \
    }                                   

#else

    #define stack_assert(stk) {;}

#endif

#define UNWRAP(val) { if (val != res::OK) { return val; } }

#endif // STACK_H