#ifndef STACK_H
#define STACK_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "log.h"

const unsigned char __const_memory_val = 228;
const void *const POISON_PTR = &__const_memory_val;

typedef unsigned int err_flags;

#define UNWRAP(val) { if (val != res::OK) { return val; } }

enum res
{
    OK              = 0,
    NULLPTR         = 1<<0,
    OVER_FILLED     = 1<<1,
    POISONED        = 1<<2,
    NOMEM           = 1<<3,
    EMPTY           = 1<<4,
    BAD_CAPACITY    = 1<<5
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
    size_t reserved;

    #ifndef NDEBUG
    const stack_debug_t *debug_data;
    #endif
};

res __stack_ctor (stack_t *stk, size_t obj_size, size_t capacity = 0);

#ifndef NDEBUG
res __stack_ctor_with_debug (stack_t *stk, const stack_debug_t *debug_data,
                                size_t obj_size, size_t capacity = 0);
#endif

#ifndef NDEBUG

    #define stack_ctor(stk, obj_size, ...)                                  \
    {                                                                       \
        const static stack_debug_t debug_info = {__PRETTY_FUNCTION__, __FILE__,    \
                                            #stk, __LINE__};                \
        __stack_ctor_with_debug (stk, &debug_info, obj_size, ##__VA_ARGS__);\
    }

#else

    #define stack_ctor(stk, obj_size, ...)                  \
    {                                                       \
        __stack_ctor(stk, obj_size, ##__VA_ARGS__);           \
    }

#endif

res stack_resize (stack_t *stk, size_t new_capacity);

res shrink_to_fit (stack_t *stk);

res push (stack_t *stk, const void *value);

res pop (stack_t *stk, void *value);

res stack_dtor (stack_t *stk);

void stack_dump (const stack_t *stk, FILE *stream);

unsigned int stack_verify (const stack_t *stk);


#ifndef NDEBUG

    #define stack_assert(stk)                                   \
    {                                                           \
        unsigned int check_res = stack_verify(stk);             \
        if (check_res != res::OK)                               \
        {                                                       \
            log(log::ERR,                                       \
                "Failed stack check with err flags: 0x%x",      \
                                            check_res);         \
            stack_dump(stk, get_log_stream());                  \
            assert (0 && "Bad stack, check logs");              \
        }                                                       \
    }                                   

#else

    #define stack_assert(stk) {;}

#endif

#endif // STACK_H