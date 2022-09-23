#ifndef STACK_H
#define STACK_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "log.h"

const unsigned char __const_memory_val = 228;
const void *POISON_PTR = &__const_memory_val;

#define UNWRAP(val) { if (val != res::OK) { return val; } }

enum res
{
    OK              = 0,
    NULLPTR         = 1<<0,
    OVER_FILLED     = 1<<1,
    POISONED        = 1<<2,
    NOMEM           = 1<<3,
    EMPTY           = 1<<4
};

struct stack_debug_t
{
    const char *const func_name;
    const char *const file;
    const char *const var_name;
    unsigned int line;
};


struct stack_t
{
    void *data;
    size_t size;
    size_t capacity;
    size_t obj_size;

    #ifndef NDEBUG
    stack_debug_t *debug_data;
    #endif
};

res __stack_ctor (stack_t *stk, size_t obj_size, size_t capacity);

#ifndef NDEBUG

    #define stack_ctor(stk, obj_size, ...)                          \
    {                                                               \
        res retcode = __stack_ctor(stk, obj_size, ##__VA_ARGS__);   \
        if (retcode != res::OK) { return retcode; }                 \
                                                                    \
        stk->debug_data = calloc (1, sizeof (stack_debug_t));       \
        if (stk->debug_data == nullptr) { return res::NOMEM; }      \
                                                                    \
        stk->debug_data.func_name = __PRETTY_FUNCTION__;            \
        stk->debug_data.file = __FILE__;                            \
        stk->debug_data.var_name = #stk;                            \
        stk->debug_data.line = __LINE__;                            \
    }

#else

    #define stack_ctor(stk, obj_size, ...)                  \
    {                                                       \
        __stack_ctor(stk, obj_size, ##__VA_ARGS);           \
    }

#endif

res stack_resize (stack_t *stk, size_t new_capacity);

res shrink_to_fit (stack_t *stk);

res push (stack_t *stk, const void *value);

res pop (stack_t *stk, void *value);

res stack_dtor (stack_t *stk);

void stack_dump (const stack_t *stk, FILE *stream);

int stack_verify (const stack_t *stk);


#ifndef NDEBUG

    #define stack_assert(stk)                           \
    {                                                   \
        if (stack_verify(stk) != res::OK)               \
        {                                               \
            fprintf (get_log_stream(), "Failed stack check at %s at file %s:(%d)", __func__, __FILE__, __LINE__); \
            stack_dump(stk, get_log_stream());          \
            assert (0 && "Bad stack, check logs");      \
        }                                               \
    }                                   

#else

    #define stack_assert(stk) {;}

#endif

#endif // STACK_H