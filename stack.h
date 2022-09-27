#ifndef STACK_H
#define STACK_H

//  ---------------- Protection options ----------------
#ifndef STACK_KSP_PROTECT
#define STACK_KSP_PROTECT               1 // Poison protection
#endif

#ifndef STACK_DUNGEON_MASTER_PROTECT
#define STACK_DUNGEON_MASTER_PROTECT    1 // Canary protection
#endif

#ifndef STACK_HASH_PROTECT
#define STACK_HASH_PROTECT              1 // Hash protection
#endif

#ifndef STACK_MEMORY_PROTECT
#if (__linux__ || __unix__)
    #define STACK_MEMORY_PROTECT            1 // Memory protection
#else
    #define STACK_MEMORY_PROTECT            0
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "log.h"

#if STACK_HASH_PROTECT
#include "hash.h"
#endif

// ---------------- Types ----------------
typedef unsigned short err_flags;
typedef void (*elem_print_f) (const void *elem, size_t elem_size, FILE *stream);
typedef uint64_t dungeon_master_t;

enum res
{
    OK                  = 0,
    NULLPTR             = 1<<0,
    INVALID_SIZE        = 1<<1,
    POISONED            = 1<<2,
    NOMEM               = 1<<3,
    EMPTY               = 1<<4,
    BAD_CAPACITY        = 1<<5,
    DATA_CORRUPTED      = 1<<6,
    STRUCT_CORRUPTED    = 1<<7,
    INVALID_OBJ_SIZE    = 1<<8,
    INVALID_FUNC        = 1<<9,
    DATA_NULL           = 1<<10
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
    #if STACK_DUNGEON_MASTER_PROTECT
    dungeon_master_t two_blocks_up;
    #endif

    void *data;
    size_t size;
    size_t capacity;
    size_t obj_size;
    size_t reserved;

    #ifndef NDEBUG
    elem_print_f print_func;
    const stack_debug_t *debug_data;
    #endif

    #if STACK_HASH_PROTECT
    hash_f hash_func;
    hash_t data_hash;
    hash_t struct_hash;
    #endif

    #if STACK_MEMORY_PROTECT
    stack_t *struct_copy; /// Without data
    #endif

    #if STACK_DUNGEON_MASTER_PROTECT
    dungeon_master_t two_blocks_down;
    #endif
};

// ---------------- Functions ----------------
err_flags __stack_ctor (stack_t *stk, size_t obj_size, size_t capacity = 0
#ifndef NDEBUG
    , elem_print_f print_func = nullptr 
#endif
#if STACK_HASH_PROTECT    
    , hash_f hash_func = nullptr
#endif
);

#ifndef NDEBUG
    err_flags __stack_ctor_with_debug (stack_t *stk, const stack_debug_t *debug_data,
                                    size_t obj_size, size_t capacity = 0, elem_print_f print_func = nullptr);

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

void stack_dump (stack_t *stk, FILE *stream);

void stack_perror (err_flags errors, FILE *stream, const char *prefix = nullptr);

#if STACK_HASH_PROTECT
err_flags stack_verify (stack_t *stk_mutable); // We need mutable stk for hash
#else
err_flags stack_verify (const stack_t *stk);
#endif

void byte_fprintf (const void *elem, size_t elem_size, FILE *stream);

// ---------------- Macros ----------------

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