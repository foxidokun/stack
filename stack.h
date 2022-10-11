#ifndef STACK_H
#define STACK_H

//  ---------------- Protection options ----------------
#ifndef STACK_KSP_PROTECT
/**
 * @brief Poison protection
 * 
 * Invariants:
 * 1) All not used bytes in data are poisoned
 * 2) Not all bytes of existing element in data are poisoned.
 * 3) Data ptr != poison_ptr until desctructor.
 */
#define STACK_KSP_PROTECT               1
#endif

#ifndef STACK_DUNGEON_MASTER_PROTECT
/**
 * @brief Canary protection
 * 
 * Invariants:
 * 1) two_blocks_up & two_blocks_down struct fields must always be equal to dungeon_master_val
 * 2) Canary blocks at the beginning and end of stack_t.data must always be equal to dungeon_master_val
 */
#define STACK_DUNGEON_MASTER_PROTECT    1
#endif

#ifndef STACK_HASH_PROTECT
/**
 * @brief Hash protection
 * 
 * Method: Structure stores its own hash and data hash. The hashes are computed using a given hash function or, if none given, using djb2.
 */
#define STACK_HASH_PROTECT              1
#endif

/**
 * @brief Memory protection
 * 
 * Method: 
 * The data and struct copy are allocated using the mmap system call.
 * Reserved capacity is equal to max(PAGE_SIZE / object_size, requested capacity).
 * Struct copy and data memory pages permissions are set READ_ONLY via mprotect syscall.
 */
#ifndef STACK_MEMORY_PROTECT
#if (__linux__ || __unix__)
    #define STACK_MEMORY_PROTECT        1
#else
    #define STACK_MEMORY_PROTECT        0
#endif
#endif

#ifndef VERBOSE_DUMP_LEVEL
#define VERBOSE_DUMP_LEVEL              0
#endif

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "log.h"
#include "hash.h"

// ---------------- Types ----------------
/// Return type. Bit OR of errors (enum res)
typedef unsigned short err_flags;
/// Function type for printing elements in dump
typedef void (*elem_print_f) (const void *elem, size_t elem_size, FILE *stream);
/// Canary type
typedef uint64_t dungeon_master_t;

/// Error enum
enum res
{
    /// Everything is fine™
    OK                  = 0,        
    /// Struct is nullptr
    NULLPTR             = 1 << 0,   
    /// Illegal size (size>capacity)
    INVALID_SIZE        = 1 << 1,   
    /// Struct is poisoned
    POISONED            = 1 << 2,   
    /// Out of memory (calloc/mmap failure)
    NOMEM               = 1 << 3,   
    /// Pop from empty stack
    EMPTY               = 1 << 4,   
    /// Bad capacity (capacity < reserved)
    BAD_CAPACITY        = 1 << 5,   
    /// Internal data buffer corrupted
    DATA_CORRUPTED      = 1 << 6,   
    /// Struct is corrupted
    STRUCT_CORRUPTED    = 1 << 7,   
    /// Invalid object size (==0)
    INVALID_OBJ_SIZE    = 1 << 8,   
    /// Invalid func (hash_func or print_func)
    INVALID_FUNC        = 1 << 9,   
    /// Data pointer is null
    DATA_NULL           = 1 << 10   
};

#ifndef NDEBUG
/// Struct for debug data
struct stack_debug_t
{
    const char *const func_name;    /// Function where stack is allocated
    const char *const file;         /// File where stack is allocated
    const char *const var_name;     /// Variable name for which the constructor was called
    unsigned int line;              /// Line where stack is allocated
};
#endif

/// Stack struct
struct stack_t
{
    #if STACK_DUNGEON_MASTER_PROTECT
    dungeon_master_t two_blocks_up;     /// Struct canary
    #endif

    void *data;                         /// Stack data
    size_t size;                        /// Stack size (used)
    size_t capacity;                    /// Stack allocated capacity
    size_t obj_size;                    /// Stack object size
    size_t reserved;                    /// Reserved capacity

    #ifndef NDEBUG
    elem_print_f print_func;            /// Function for printing elements
    const stack_debug_t *debug_data;    /// Debug data
    #endif

    #if STACK_HASH_PROTECT
    hash_f hash_func;                   /// Hash function
    hash_t data_hash;                   /// Data hash
    hash_t struct_hash;                 /// Struct hash (calculated with struct_hash=0)
    #endif

    #if STACK_MEMORY_PROTECT
    stack_t *struct_copy;               /// Struct copy without own data
    #endif

    #if STACK_DUNGEON_MASTER_PROTECT
    dungeon_master_t two_blocks_down;   /// Struct canary
    #endif
};

// ---------------- Functions ----------------
/**
 * @brief      Stack constructor
 *
 * @param[out] stk         Pointer to stack
 * @param[in]  obj_size    Object size
 * @param[in]  capacity    Reserved capacity
 * @param[in]  print_func  Function for printing elements (can be nullptr -> per byte print)
 * @param[in]  hash_func   Hash function (can be nullptr -> djb2)
 *
 * @return     Error flags (bitor of res enum)
 */
err_flags __stack_ctor (stack_t *stk, size_t obj_size, size_t capacity = 0, elem_print_f print_func = nullptr, hash_f hash_func = nullptr);

#ifndef NDEBUG
    err_flags __stack_ctor_with_debug (stack_t *stk, const stack_debug_t *debug_data,
                                    size_t obj_size, size_t capacity = 0, elem_print_f print_func = nullptr, hash_f hash_func = nullptr);

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

///@brief      Print errors description with given prefix to stream
void stack_perror (err_flags errors, FILE *stream, const char *prefix = nullptr);

err_flags stack_verify (stack_t *stk_mutable); // We need mutable stk for hash

/// Print element bytes
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