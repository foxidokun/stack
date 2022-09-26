#define STACK_CPP

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "log.h"
#include "stack.h"

const unsigned char __const_memory_val = 228;
const void *const POISON_PTR = &__const_memory_val;
const unsigned char POISON_BYTE = (unsigned char) -7u;

const dungeon_master_t dungeon_master_val = 0x1000DEAD7;

#if STACK_HASH_PROTECT
err_flags stack_verify (stack_t *stk_mutable) { // We need not const stk for hash
    const stack_t *stk = (const stack_t *) stk_mutable;
#else
err_flags stack_verify (const stack_t *stk) {
#endif

    err_flags ret = res::OK;
    const err_flags data_is_not_ok = DATA_NULL | DATA_CORRUPTED | POISONED | BAD_CAPACITY | INVALID_OBJ_SIZE | STRUCT_CORRUPTED;

    if (stk == nullptr) return res::NULLPTR;

    if (stk->size > stk->capacity)      ret |= res::INVALID_SIZE;
    if (stk->capacity < stk->reserved)  ret |= res::BAD_CAPACITY;
    if (stk->obj_size == 0)             ret |= res::INVALID_OBJ_SIZE;
    if (stk->data == nullptr)           ret |= res::DATA_NULL;

    #ifndef NDEBUG 
        if (stk->print_func == nullptr) ret |= res::INVALID_FUNC;
    #endif

    #if STACK_KSP_PROTECT
        if (!(ret & (data_is_not_ok | INVALID_SIZE)))
        {
            ret |= data_poison_check (stk);
        } 
    #endif

    #if STACK_DUNGEON_MASTER_PROTECT
        if (stk->two_blocks_up != dungeon_master_val || stk->two_blocks_down != dungeon_master_val)
        {
            ret |= STRUCT_CORRUPTED;
        }

        if (!(ret & data_is_not_ok))
        {
            if (((dungeon_master_t *)stk->data)[-1] != dungeon_master_val)
            {
                ret |= DATA_CORRUPTED;
            }

            if (* (dungeon_master_t *) ((char *)stk->data + stk->capacity * stk->obj_size) != dungeon_master_val)
            {
                ret |= DATA_CORRUPTED;
            }
        }
    #endif

    #if STACK_HASH_PROTECT
        if (stk->hash_func == nullptr) { ret |= INVALID_FUNC; }
        else {
            const hash_t struct_hash = stk->struct_hash;
            stk_mutable->struct_hash = 0;

            if (stk->hash_func (stk, sizeof (stack_t)) != struct_hash)
            {
                ret |= STRUCT_CORRUPTED;
            }
            stk_mutable->struct_hash = struct_hash;

            size_t data_size = stk->capacity * stk->obj_size;
            if ((!(ret & data_is_not_ok)) & (stk->hash_func (stk->data, data_size) != stk->data_hash))
            {
                ret |= DATA_CORRUPTED;
            }
        }
    #endif

    return ret;
}

err_flags data_poison_check (const stack_t *stk)
{
    assert (stk != nullptr && "In this function stk can't be null");
    assert (stk->data != nullptr && "Stack data in this function can't be null");

    if (stk->data == POISON_PTR)
        return POISONED;

    for (size_t n = stk->size; n < stk->capacity; ++n)
    {
        for (size_t i = 0; i < stk->obj_size; ++i)
        {
            if (((unsigned char *)stk->data)[stk->obj_size*n+i] != POISON_BYTE)
            {
                return res::DATA_CORRUPTED;
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

        if (elem_poisoned) return POISONED;
    }

    return OK;
}

err_flags __stack_ctor (stack_t *stk, size_t obj_size, size_t capacity
#ifndef NDEBUG
    , elem_print_f print_func
#endif
#if STACK_HASH_PROTECT    
    , hash_f hash_func
#endif
)
{
    assert (obj_size > 0 && "object size cant be 0");

    #if STACK_DUNGEON_MASTER_PROTECT
        void *mem_ptr = calloc (capacity*obj_size + 2*sizeof (dungeon_master_t), 1);
    #else
        void *mem_ptr = calloc (capacity, obj_size); // Works even with capacity = 0
    #endif

    if (mem_ptr ==  nullptr) { return res::NOMEM; }

    stk->data     = mem_ptr;
    stk->capacity = capacity;
    stk->reserved = capacity;
    stk->size     = 0;
    stk->obj_size = obj_size;

    #ifndef NDEBUG
        if (print_func != nullptr)  stk->print_func = print_func;
        else                        stk->print_func = byte_fprintf;
    #endif

    #if STACK_DUNGEON_MASTER_PROTECT
        stk->two_blocks_up   = dungeon_master_val;
        stk->two_blocks_down = dungeon_master_val;

        *(dungeon_master_t*)stk->data = dungeon_master_val;
        stk->data = (dungeon_master_t*)stk->data + 1;
        * (dungeon_master_t *) ((char *)stk->data + stk->capacity * stk->obj_size) = dungeon_master_val;
    #endif

    #if STACK_KSP_PROTECT
        memset (stk->data, POISON_BYTE, capacity*obj_size);
    #endif

    #if STACK_HASH_PROTECT
        stk->hash_func   = (hash_func != nullptr) ? hash_func : djb2;
        stk->struct_hash = 0;
        stk->data_hash   = stk->hash_func (stk->data, stk->capacity * stk->obj_size);
        stk->struct_hash = stk->hash_func (stk, sizeof (stack_t));
    #endif

    stack_assert (stk);

    return res::OK;
}

#ifndef NDEBUG
err_flags __stack_ctor_with_debug (stack_t *stk, const stack_debug_t *debug_data,
                                size_t obj_size, size_t capacity, elem_print_f print_func)
{
    assert (stk != nullptr && "pointer can't be NULL");

    stk->debug_data = debug_data;

    return __stack_ctor (stk, obj_size, capacity, print_func);
}
#endif

err_flags stack_resize (stack_t *stk, size_t new_capacity)
{
    stack_assert (stk);
    assert (stk->size <= new_capacity);

    #if STACK_DUNGEON_MASTER_PROTECT
        stk->data = ((dungeon_master_t*) stk->data) - 1;
    #endif

    #if STACK_DUNGEON_MASTER_PROTECT
        void *new_data_ptr = realloc (stk->data, new_capacity*stk->obj_size + 2*sizeof (dungeon_master_t)); // Не заполняет нулями
    #else
        void *new_data_ptr = realloc (stk->data, new_capacity*stk->obj_size); // Не заполняет нулями
    #endif

    if (new_data_ptr == nullptr) return res::NOMEM;

    #if STACK_DUNGEON_MASTER_PROTECT
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

    #if STACK_HASH_PROTECT
        stk->struct_hash = 0;
        stk->data_hash   = stk->hash_func (stk->data, stk->capacity * stk->obj_size);
        stk->struct_hash = stk->hash_func (stk, sizeof (stack_t));
    #endif

    stack_assert (stk);
    return res::OK;
}

err_flags stack_shrink_to_fit (stack_t *stk)
{
    stack_assert (stk);

    stack_resize (stk, stk->size);

    stack_assert (stk);
    return res::OK;
}

err_flags stack_pop (stack_t *stk, void *value)
{
    stack_assert (stk);
    assert (value != nullptr && "pointer can't be NULL");

    if (stk->size == 0)
    {
        return res::EMPTY;
    }
    stk->size--;
    memcpy (value, (char* ) stk->data + stk->size*stk->obj_size, stk->obj_size);

    #if STACK_KSP_PROTECT
        memset ((char* ) stk->data + stk->size*stk->obj_size, POISON_BYTE, stk->obj_size);
    #endif

    #if STACK_HASH_PROTECT
        stk->struct_hash = 0;
        stk->data_hash   = stk->hash_func (stk->data, stk->capacity * stk->obj_size);
        stk->struct_hash = stk->hash_func (stk, sizeof (stack_t));
    #endif

    if (stk->capacity>>2 >= stk->size)
    {
        if (stk->capacity>>1 > stk->reserved)
        {
            UNWRAP (stack_resize (stk, stk->capacity>>1));
        }
        else
        {
            UNWRAP (stack_resize (stk, stk->reserved));
        }
    }

    stack_assert (stk);
    return res::OK;
}

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

    memcpy ((char* ) stk->data + stk->size*stk->obj_size, value, stk->obj_size);
    stk->size++;

    #if STACK_HASH_PROTECT
        stk->struct_hash = 0;
        stk->data_hash   = stk->hash_func (stk->data, stk->capacity * stk->obj_size);
        stk->struct_hash = stk->hash_func (stk, sizeof (stack_t));
    #endif

    stack_assert (stk);
    return res::OK;
}

err_flags stack_dtor (stack_t *stk)
{
    if (stk == nullptr) { return res::OK; }

    #ifndef NDEBUG
        err_flags check_res = stack_verify (stk);
        if (check_res != OK) log(log::WRN, "Destructor called on invalid object with error flags: 0x%x, see stack_perror", check_res);
    #endif

    #if STACK_KSP_PROTECT
        memset ((char* ) stk->data, POISON_BYTE, stk->obj_size * stk->capacity);
    #endif

    #if STACK_DUNGEON_MASTER_PROTECT
        stk->data = ((dungeon_master_t*) stk->data) - 1;
    #endif

    free (stk->data);

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

void stack_dump (stack_t *stk, FILE *stream)
{
    fprintf (stream, R Bold "\n======== STACK DUMP =======\n" Plain D);

    if (stk == nullptr)
    {
        fprintf (stream, "Stack ptr is nullptr\n");
        return;
    }

    err_flags check_res = stack_verify (stk);

    if (check_res & res::POISONED)
    {
        fprintf (stream, "Stack " R "POISONED" D "\n");
        return;
    }

    if (check_res != OK)
    {
        fprintf (stream, "Stack has errors: \n");
        stack_perror(check_res, stream, "-> ");
    }

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
    fprintf (stream, "Stack data[%p]\n", stk->data);

    for (size_t i = 0; i < stk->capacity; ++i)
    {
        fprintf (stream, "%c data[%03lu]: ", (i<stk->size ? '*' : ' '), i);

        #ifndef NDEBUG
            stk->print_func ((char *) stk->data + i*stk->obj_size, stk->obj_size, stream);
        #else
            byte_fprintf((char *) stk->data + i*stk->obj_size, stk->obj_size, stream);
        #endif
    }

    fprintf (stream, R Bold "======== END STACK DUMP =======\n\n" Plain D);
}

#define _if_log(res, message)                                  \
{                                                              \
    if (errors & res)                                          \
        fprintf (stream, "%s" message "\n", prefix ? prefix : "");  \
}

void stack_perror (err_flags errors, FILE *stream, const char *prefix)
{
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

void byte_fprintf (void *elem, size_t elem_size, FILE *stream)
{
    #if STACK_KSP_PROTECT
        bool is_poison = true;
    #else
        bool is_poison = false;
    #endif

    for (size_t j = 0; j < elem_size; ++j)
    {   
        #if STACK_KSP_PROTECT
            if (((unsigned char *)elem)[j] != POISON_BYTE) 
            {
                is_poison = false;
            }
        #endif

        fprintf (stream, "|0x%08x|", ((unsigned char *)elem)[j]);
    }

    if (is_poison)
    {
        fprintf (stream, "(POISON)"); 
    }

    fprintf (stream, "\n");
}