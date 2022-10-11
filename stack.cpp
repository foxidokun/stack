#define STACK_CPP

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "log.h"
#include "stack.h"

#if STACK_MEMORY_PROTECT
#include <sys/mman.h>
#include <unistd.h>
#endif

// ---- ---- ---- --- CONSTS ---- ---- ---- ----
#if STACK_KSP_PROTECT
/// Random const variable
const unsigned char __const_memory_val = 228;
/// Pointer to const memory
const void *const POISON_PTR = &__const_memory_val;
const unsigned char POISON_BYTE = (unsigned char) -7u;
#endif

const err_flags DATA_NOT_OKAY = DATA_NULL | DATA_CORRUPTED | POISONED | BAD_CAPACITY | INVALID_OBJ_SIZE | STRUCT_CORRUPTED;

/// Canary value
const dungeon_master_t dungeon_master_val = 0x1000DEAD7;

// ---- ---- ---- --- PROTOTYPES ---- ---- ---- ----

/// Memory protection: mprotect wrappers with #ifdef compilation
static inline void unlock_copy (stack_t *stk);
static inline void   lock_copy (stack_t *stk);
static inline void unlock_data (stack_t *stk);
static inline void   lock_data (stack_t *stk);

static inline void update_hash (stack_t *stk);

/// Protection checkers with #ufdef compilation
static void dungeon_master_check (const stack_t *stk, err_flags *errs);
static void data_poison_check    (const stack_t *stk, err_flags *errs);
static void hash_check           (      stack_t *stk, err_flags *errs);
static void memory_check         (const stack_t *stk, err_flags *errs);

static size_t get_data_size (size_t capacity, size_t obj_size);

static void *cust_realloc (void *prev_ptr, size_t prev_size, size_t new_size);

static err_flags stack_data_init (stack_t *stk, size_t reserved, size_t obj_size);
static void init_dungeon_master_protection (stack_t *stk);

// ---- ---- ---- --- IMPLEMENTATIONS ---- ---- ---- ----

err_flags stack_verify (stack_t *stk_mutable) // We need mutable stk for hash
{ 
    const stack_t *stk = (const stack_t *) stk_mutable;

    err_flags ret = res::OK;

    if (stk == nullptr) return res::NULLPTR;

    if (stk->size > stk->capacity)      ret |= res::INVALID_SIZE;
    if (stk->capacity < stk->reserved)  ret |= res::BAD_CAPACITY;
    if (stk->obj_size == 0)             ret |= res::INVALID_OBJ_SIZE;
    if (stk->data == nullptr)           ret |= res::DATA_NULL;

    #ifndef NDEBUG 
        if (stk->print_func == nullptr) ret |= res::INVALID_FUNC;
    #endif

    data_poison_check (stk, &ret);
    dungeon_master_check (stk, &ret);
    hash_check (stk_mutable, &ret);
    memory_check (stk, &ret);

    return ret;
}

// ------------------------------------------------------------------------------------

err_flags __stack_ctor (stack_t *stk, size_t obj_size, size_t capacity, elem_print_f print_func, hash_f hash_func)
{
    assert (obj_size > 0   && "object size cant be 0");
    assert (stk != nullptr && "pointer can't be null");

    // Data & fields initialisation
    stack_data_init (stk, capacity, obj_size);

    // Protection initialising
    #ifndef NDEBUG
        if (print_func != nullptr)  stk->print_func = print_func;
        else                        stk->print_func = byte_fprintf;
    #endif

    init_dungeon_master_protection (stk);

    #if STACK_KSP_PROTECT
        memset (stk->data, POISON_BYTE, stk->capacity*obj_size);
    #endif

    #if STACK_HASH_PROTECT
        stk->hash_func   = (hash_func != nullptr) ? hash_func : djb2;
    #endif
    
    #if STACK_MEMORY_PROTECT
        memcpy (stk->struct_copy, stk, sizeof (stack_t));
    #endif

    update_hash (stk);

    lock_data   (stk);
    lock_copy (stk);

    stack_assert (stk);

    return res::OK;
}

// ------------------------------------------------------------------------------------

#ifndef NDEBUG
err_flags __stack_ctor_with_debug (stack_t *stk, const stack_debug_t *debug_data,
                                size_t obj_size, size_t capacity, elem_print_f print_func, hash_f hash_func)
{
    assert (stk != nullptr && "pointer can't be NULL");

    stk->debug_data = debug_data;

    return __stack_ctor (stk, obj_size, capacity, print_func, hash_func);
}
#endif

// ------------------------------------------------------------------------------------

err_flags stack_resize (stack_t *stk, size_t new_capacity)
{
    stack_assert (stk);
    assert (stk->size <= new_capacity);
    
    if (new_capacity < stk->reserved)
    {
        return res::BAD_CAPACITY;
    }

    size_t new_data_size = get_data_size (new_capacity, stk->obj_size);

    #if STACK_DUNGEON_MASTER_PROTECT
    stk->data = ((dungeon_master_t*) stk->data) - 1;
    #endif

    void *new_data_ptr = cust_realloc (stk->data, get_data_size (stk->capacity, stk->obj_size), new_data_size);
    if (new_data_ptr == nullptr) return res::NOMEM;

    #if STACK_DUNGEON_MASTER_PROTECT
        #if STACK_MEMORY_PROTECT
        mprotect (new_data_ptr, new_data_size, PROT_WRITE|PROT_READ);
        #endif
        new_data_ptr = ((dungeon_master_t*) new_data_ptr) + 1;
        * ((dungeon_master_t *) ((char *)new_data_ptr + new_capacity * stk->obj_size)) = dungeon_master_val;
    #endif

    #if STACK_KSP_PROTECT
        if (new_capacity > stk->capacity)
        {
            memset ((char* ) new_data_ptr + stk->capacity*stk->obj_size, POISON_BYTE, (new_capacity - stk->capacity)*stk->obj_size);
        }
    #endif

    stk->data     = new_data_ptr;
    stk->capacity = new_capacity;

    #if STACK_MEMORY_PROTECT
        unlock_copy (stk);
        stk->struct_copy->data     = stk->data;
        stk->struct_copy->capacity = stk->capacity;
        lock_copy (stk);
        lock_data (stk);
    #endif

    update_hash (stk);

    stack_assert (stk);
    return res::OK;
}

// ------------------------------------------------------------------------------------

err_flags stack_shrink_to_fit (stack_t *stk)
{
    stack_assert (stk);

    stack_resize (stk, stk->size);

    stack_assert (stk);
    return res::OK;
}

// ------------------------------------------------------------------------------------

err_flags stack_pop (stack_t *stk, void *value)
{
    stack_assert (stk);
    assert (value != nullptr && "pointer can't be NULL");

    if (stk->size == 0)
    {
        return res::EMPTY;
    }

    stk->size--;
    #if STACK_MEMORY_PROTECT
        unlock_copy (stk);
        stk->struct_copy->size--;    
        lock_copy (stk);
    #endif

    memcpy (value, (char* ) stk->data + stk->size*stk->obj_size, stk->obj_size);

    #if STACK_KSP_PROTECT
        unlock_data (stk);
        memset ((char* ) stk->data + stk->size*stk->obj_size, POISON_BYTE, stk->obj_size);
        lock_data (stk);
    #endif

    update_hash (stk);

    if (stk->capacity >> 2 >= stk->size)
    {
        if (stk->capacity >> 1 > stk->reserved)
        {
            UNWRAP (stack_resize (stk, stk->capacity >> 1));
        }
        else
        {
            UNWRAP (stack_resize (stk, stk->reserved));
        }
    }

    stack_assert (stk);
    return res::OK;
}

// ------------------------------------------------------------------------------------

err_flags stack_push (stack_t *stk, const void *value)
{
    stack_assert (stk);
    assert (value != nullptr && "pointer can't be null");

    if (stk->size == stk->capacity)
    {
        if (stk->capacity == 0)
        {
            UNWRAP (stack_resize (stk, 1));
        }
        else
        {
            UNWRAP (stack_resize (stk, stk->capacity<<1));
        }
    }

    unlock_data (stk);
    memcpy ((char* ) stk->data + stk->size*stk->obj_size, value, stk->obj_size);
    lock_data (stk);

    stk->size++;
    #if STACK_MEMORY_PROTECT
        unlock_copy (stk);
        stk->struct_copy->size++;
        lock_copy (stk);
    #endif

    update_hash (stk);

    stack_assert (stk);
    return res::OK;
}

// ------------------------------------------------------------------------------------

err_flags stack_dtor (stack_t *stk)
{
    if (stk == nullptr) { return res::OK; }

    #ifndef NDEBUG
        err_flags check_res = stack_verify (stk);
        if (check_res != OK) log(log::WRN, "Destructor called on invalid object with error flags: 0x%x, see stack_perror", check_res);
    #endif

    #if STACK_KSP_PROTECT
        unlock_data (stk);
        memset ((char* ) stk->data, POISON_BYTE, stk->obj_size * stk->capacity);
    #endif

    #if STACK_DUNGEON_MASTER_PROTECT
        stk->data = ((dungeon_master_t*) stk->data) - 1;
    #endif

    #if STACK_MEMORY_PROTECT
        munmap (stk->data,        get_data_size (stk->capacity, stk->obj_size));
        munmap (stk->struct_copy, sizeof (stack_t));
    #else
        free (stk->data);
    #endif

    #if STACK_KSP_PROTECT
        // Poisoning
        stk->data     = const_cast<void *>(POISON_PTR); // Write to POISON_PTR is SegFault, but this is main idea of poisoning
        stk->size     = -1u;
        stk->capacity =   0;
        stk->obj_size =   0;
        stk->reserved = -1u;
    #endif

    return res::OK;
}

// ------------------------------------------------------------------------------------

void stack_dump (stack_t *stk, FILE *stream)
{
    fprintf (stream, R Bold "\n======== STACK DUMP =======\n" Plain D);

    if (stk == nullptr)
    {
        fprintf (stream, "Stack ptr is nullptr\n");
        return;
    }

    err_flags check_res = stack_verify (stk);

    if (check_res != OK)
    {
        fprintf (stream, "Stack has errors: \n");
        stack_perror(check_res, stream, "-> ");
    }

    if (check_res & res::POISONED) { return; }

    #ifndef NDEBUG
        fprintf (stream, "Stack[%p] with name " Bold "%s" Plain 
            " allocated at " Bold "%s" Plain " at file " Bold "%s:(%u)\n" Plain,
            stk, stk->debug_data->var_name, stk->debug_data->func_name, stk->debug_data->file, stk->debug_data->line
        );
    #else
        fprintf (stream, "Stack[%p]\n", stk);
    #endif

    fprintf (stream, "Parameters:\n"
                     "    size: %lu\n"
                     "    capacity: %lu\n"
                     "    object size: %lu\n"
                     "    reserved size: %lu\n\n",
                     stk->size, stk->capacity, stk->obj_size, stk->reserved);
    fprintf (stream, "Enabled security options:\n");
    fprintf (stream, "[%c] Memory protection\n", STACK_MEMORY_PROTECT         ? '+' : '-');
    fprintf (stream, "[%c] Canary protection\n", STACK_DUNGEON_MASTER_PROTECT ? '+' : '-');
    fprintf (stream, "[%c] Hash protection\n",   STACK_HASH_PROTECT           ? '+' : '-');
    fprintf (stream, "[%c] Poison protection\n", STACK_KSP_PROTECT            ? '+' : '-');
    fprintf (stream, "\nStack data[%p]\n", stk->data);

    #if VERBOSE_DUMP_LEVEL
        size_t max_index = stk->capacity;
    #else
        size_t max_index = stk->size;
    #endif

    for (size_t i = 0; i < max_index; ++i)
    {
        fprintf (stream, "%c data[%03lu]: ", (i<stk->size ? '*' : ' '), i);

        #ifndef NDEBUG
            stk->print_func ((char *) stk->data + i*stk->obj_size, stk->obj_size, stream);
        #else
            byte_fprintf((char *) stk->data + i*stk->obj_size, stk->obj_size, stream);
        #endif

        #if STACK_KSP_PROTECT
            bool is_poison = true;
            for (size_t j = 0; j < stk->obj_size; ++j)
            {   
                if (*((unsigned char *) stk->data + i*stk->obj_size +j) != POISON_BYTE) 
                {
                    is_poison = false;
                }
            }

            if (is_poison) fprintf (stream, "%s (POISON)" D, (i < stk->size) ? R : Cyan);
        #endif   

        fputc ('\n', stream);
    }

    fprintf (stream, R Bold "======== END STACK DUMP =======\n\n" Plain D);
}

// ------------------------------------------------------------------------------------

#define _if_log(res, message)                                                \
{                                                                            \
    if (errors & res)                                                        \
        fprintf (stream, "%s" message "\n", prefix==nullptr ? prefix : "");  \
}

void stack_perror (err_flags errors, FILE *stream, const char *prefix)
{
    assert (stream != nullptr && "pointer can't be null");

    _if_log (NULLPTR         , "Stack pointer is nullptr");
    _if_log (INVALID_SIZE    , "Used > capacity");
    _if_log (POISONED        , "Use after deconstructor");
    _if_log (NOMEM           , "Out of memory");
    _if_log (EMPTY           , "Pop from empty stack");
    _if_log (BAD_CAPACITY    , "Capacity < reserved");
    _if_log (DATA_CORRUPTED  , "Internal data buffer is corrupted");
    _if_log (STRUCT_CORRUPTED, "Struct is corrupted");
    _if_log (INVALID_OBJ_SIZE, "Invalid object size = 0");
    _if_log (INVALID_FUNC    , "Nullptr function pointer");
    _if_log (DATA_NULL       , "Data pointer is nullptr");

    assert ((errors & ~(NULLPTR | INVALID_SIZE | POISONED | NOMEM | EMPTY | BAD_CAPACITY | DATA_CORRUPTED
                    | STRUCT_CORRUPTED | INVALID_OBJ_SIZE | INVALID_FUNC | DATA_NULL)) == 0 && "Unexpected error");
}

#undef _if_log

// ------------------------------------------------------------------------------------

static void data_poison_check (const stack_t *stk, err_flags *errs)
{
    assert (stk  != nullptr && "In this function stk can't be null");
    assert (errs != nullptr && "Errors pointer can't be null");

    #if STACK_KSP_PROTECT
    if ((*errs & (DATA_NOT_OKAY | INVALID_SIZE)))
    {
        return;
    } 

    if (stk->data == POISON_PTR)
    {
        *errs |= POISONED;
        return;
    }

    for (size_t n = stk->size; n < stk->capacity; ++n)
    {
        for (size_t i = 0; i < stk->obj_size; ++i)
        {
            if (((unsigned char *)stk->data)[stk->obj_size*n+i] != POISON_BYTE)
            {
                *errs |= res::DATA_CORRUPTED;
                return;
            }
        }
    }

    for (size_t n = 0; n < stk->size; ++n)
    {
        bool elem_poisoned = true;

        for (size_t i = 0; i < stk->obj_size; ++i)
        {
            if (((unsigned char *)stk->data)[stk->obj_size*n+i] != POISON_BYTE)
            {
                elem_poisoned = false;
            }
        }

        if (elem_poisoned)
        {
            *errs |= POISONED;
            return;
        }
    }

    #endif
}

// ------------------------------------------------------------------------------------

static void dungeon_master_check (const stack_t *stk, err_flags *errs)
{
    assert (stk  != nullptr && "pointer can't be null");
    assert (errs != nullptr && "pointer can't be null");

    #if STACK_DUNGEON_MASTER_PROTECT
    if (stk->two_blocks_up != dungeon_master_val || stk->two_blocks_down != dungeon_master_val)
    {
        *errs |= STRUCT_CORRUPTED;
    }

    if (!(*errs & DATA_NOT_OKAY))
    {
        if (((dungeon_master_t *)stk->data)[-1] != dungeon_master_val)
        {
            *errs |= DATA_CORRUPTED;
        }

        if (* (dungeon_master_t *) ((char *)stk->data + stk->capacity * stk->obj_size) != dungeon_master_val)
        {
            *errs |= DATA_CORRUPTED;
        }
    }
    #endif
}

// ------------------------------------------------------------------------------------

static void hash_check (stack_t *stk_mutable, err_flags *errs)
{
    assert (stk_mutable != nullptr && "Pointer can't be null");
    assert (errs        != nullptr && "Pointer can't be null");

    #if STACK_HASH_PROTECT
    const stack_t *stk = stk_mutable;

    if (stk->hash_func == nullptr) { *errs |= INVALID_FUNC; }
    else {
        const hash_t struct_hash = stk->struct_hash;
        
        stk_mutable->struct_hash = 0;

        if (stk->hash_func (stk, sizeof (stk)) != struct_hash)
        {
            *errs |= STRUCT_CORRUPTED;
        }
        stk_mutable->struct_hash = struct_hash;

        size_t data_size = stk->capacity * stk->obj_size;
        if ((!(*errs & DATA_NOT_OKAY)) & (stk->hash_func (stk->data, data_size) != stk->data_hash))
        {
            *errs |= DATA_CORRUPTED;
        }
    }
    #endif
}

// ------------------------------------------------------------------------------------

static void memory_check (const stack_t *stk, err_flags *errs)
{
    assert (stk  != nullptr && "pointer can't be null");
    assert (errs != nullptr && "pointer can't be null");

    #if STACK_MEMORY_PROTECT
        if (!(*errs & STRUCT_CORRUPTED))
        {
            if (memcmp (stk, stk->struct_copy, sizeof (stack_t)) != 0)
            {
                *errs |= STRUCT_CORRUPTED;
            }
        }
    #endif
}

// ------------------------------------------------------------------------------------

void byte_fprintf (const void *elem, size_t elem_size, FILE *stream)
{
    assert (elem   != nullptr && "pointer can't be null");
    assert (stream != nullptr && "pointer can't be null");

    #if STACK_KSP_PROTECT
        bool is_poison = true;
    #else
        bool is_poison = false;
    #endif

    const unsigned char *elem_c = (const unsigned char *) elem;

    for (size_t j = 0; j < elem_size; ++j)
    {   
        #if STACK_KSP_PROTECT
            if (elem_c[j] != POISON_BYTE) 
            {
                is_poison = false;
            }
        #endif

        fprintf (stream, "|0x%08x|", elem_c[j]);
    }

    if (is_poison)
    {
        fprintf (stream, "(POISON)"); 
    }
}

// ------------------------------------------------------------------------------------

static inline void unlock_copy (stack_t *stk)
{
    assert (stk != nullptr && "pointer can't be null");

    #if STACK_MEMORY_PROTECT
        mprotect (stk->struct_copy, sizeof (stack_t), PROT_READ | PROT_WRITE);
    #endif
}

// ------------------------------------------------------------------------------------

static inline void lock_copy (stack_t *stk)
{
    assert (stk != nullptr && "pointer can't be null");

    #if STACK_MEMORY_PROTECT
        mprotect (stk->struct_copy, sizeof (stack_t), PROT_READ);
    #endif
}

// ------------------------------------------------------------------------------------

static inline void unlock_data (stack_t *stk)
{
    assert (stk != nullptr && "pointer can't be null");

    #if STACK_MEMORY_PROTECT
        size_t data_size = stk->capacity*stk->obj_size;
        char * data = (char *) stk->data;
        #if STACK_DUNGEON_MASTER_PROTECT
            data_size += 2*sizeof (dungeon_master_t);
            data      -=   sizeof (dungeon_master_t);
        #endif

        mprotect (data, data_size, PROT_READ | PROT_WRITE);
    #endif
}

// ------------------------------------------------------------------------------------

static inline void lock_data (stack_t *stk)
{
    assert (stk != nullptr && "pointer can't be null");

    #if STACK_MEMORY_PROTECT
        size_t data_size = stk->capacity*stk->obj_size;
        char * data = (char *) stk->data;
        #if STACK_DUNGEON_MASTER_PROTECT
            data_size += 2*sizeof (dungeon_master_t);
            data      -=   sizeof (dungeon_master_t);
        #endif

        mprotect (data, data_size, PROT_READ);
    #endif
}

// ------------------------------------------------------------------------------------

static inline void update_hash (stack_t *stk)
{
    assert ((stack_verify (stk) & ~(DATA_CORRUPTED | STRUCT_CORRUPTED)) == OK);

    #if STACK_HASH_PROTECT
        stk->struct_hash = 0;
        stk->data_hash   = stk->hash_func (stk->data, stk->capacity * stk->obj_size);
        stk->struct_hash = stk->hash_func (stk, sizeof (stk));

        #if STACK_MEMORY_PROTECT
            unlock_copy (stk);
            stk->struct_copy->data_hash   = stk->data_hash;
            stk->struct_copy->struct_hash = stk->struct_hash;
            lock_copy (stk);
        #endif
    #endif

    assert (stack_verify (stk) == OK);
}

// ------------------------------------------------------------------------------------

static size_t get_data_size (size_t capacity, size_t obj_size)
{
    size_t data_size = capacity*obj_size;
    #if STACK_DUNGEON_MASTER_PROTECT
        data_size += 2*sizeof (dungeon_master_t);
    #endif

    return data_size;
}

// ------------------------------------------------------------------------------------

static void *cust_realloc (void *prev_ptr, size_t prev_size, size_t new_size)
{
    assert (prev_ptr != nullptr && "pointer can't be null"); // Due to mremap limitations

    #if STACK_MEMORY_PROTECT
        void *new_ptr = mremap (prev_ptr, prev_size, new_size, MREMAP_MAYMOVE);
        if (new_ptr == MAP_FAILED) return nullptr;
    #else
        void *new_ptr = realloc (prev_ptr, new_size); // Не заполняет нулями
    #endif

    return new_ptr;
}

// ------------------------------------------------------------------------------------

static void init_dungeon_master_protection (stack_t *stk)
{
    assert (stk       != nullptr);
    assert (stk->data != nullptr);

    #if STACK_DUNGEON_MASTER_PROTECT
        stk->two_blocks_up   = dungeon_master_val;
        stk->two_blocks_down = dungeon_master_val;

        *(dungeon_master_t*)stk->data = dungeon_master_val;
        stk->data = (dungeon_master_t*)stk->data + 1;
        * (dungeon_master_t *) ((char *)stk->data + stk->capacity * stk->obj_size) = dungeon_master_val;
    #endif
}

// ------------------------------------------------------------------------------------

static err_flags stack_data_init (stack_t *stk, size_t reserved, size_t obj_size)
{
    assert (stk != nullptr && "pointer can't be null");
    assert (obj_size > 0   && "invalid obj size");

    stk->obj_size = obj_size;

    #if STACK_MEMORY_PROTECT
        ssize_t pagesize = sysconf (_SC_PAGESIZE);
        // As the sysconf man says the only error is EINVAL (invalid name) and I'm sure _SC_PAGESIZE is the correct value.
        assert (pagesize != -1);
        size_t objects_in_mempage = ((size_t) pagesize) / obj_size;
        reserved = (reserved > objects_in_mempage) ? reserved : objects_in_mempage;
    #endif

    size_t data_size = get_data_size (reserved, obj_size);

    #if STACK_MEMORY_PROTECT
        void *mem_ptr        =             mmap (nullptr, data_size,        PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        stack_t *struct_copy = (stack_t *) mmap (nullptr, sizeof (stack_t), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

        if (struct_copy == MAP_FAILED) { return res::NOMEM; }
        if (mem_ptr == MAP_FAILED) { return res::NOMEM; }
    #else
        void *mem_ptr = calloc (data_size, 1); // Works even with capacity = 0
        if (mem_ptr == nullptr) { return res::NOMEM; }
    #endif

    // Set data pointer
    stk->data = mem_ptr;
    stk->capacity = reserved;
    stk->reserved = reserved;
    stk->size     = 0;

    #if STACK_MEMORY_PROTECT
    stk->struct_copy = struct_copy;
    #endif
    
    return res::OK;
}