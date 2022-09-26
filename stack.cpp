#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "log.h"
#include "stack.h"

#define POISON_PROTECT            1
#define DUNGEON_MASTER_PROTECT    1
#define HASH_PROTECT              1

const unsigned char __const_memory_val = 228;
const void *const POISON_PTR = &__const_memory_val;
const unsigned char POISON_BYTE = (unsigned char) -7u;

typedef uint64_t dungeon_master_t;
const dungeon_master_t dungeon_master_val = 0x300B4CC;

err_flags stack_verify (const stack_t *stk)
{
    err_flags ret = res::OK;

    if (stk == nullptr) { return res::NULLPTR; }

    if (stk->size > stk->capacity) { ret |= res::OVER_FILLED; }
    if (stk->capacity < stk->reserved) { ret |= res::BAD_CAPACITY; }

    #if POISON_PROTECT
        if (stk->obj_size == 0) { ret |= res::POISONED; };
    #endif

    if (stk->data == nullptr && (stk->size != 0 || stk->capacity != 0))
        { ret |= res::OVER_FILLED | BAD_CAPACITY; }

    #if POISON_PROTECT
        data_poison_check (stk);
    #endif

    #if DUNGEON_MASTER_PROTECT
        if (((dungeon_master_t *)stk->data)[-1] != dungeon_master_val)
        {
            ret |= DATA_CORRUPTED;
        }

        if (* (dungeon_master_t *) ((char *)stk->data + stk->capacity * stk->obj_size) != dungeon_master_val)
        {
            ret |= DATA_CORRUPTED;
        }
    #endif

    return ret;
}

err_flags data_poison_check (const stack_t *stk)
{
    assert (stk != nullptr && "In this function stk can't be null");

    if (stk->data == POISON_PTR || stk->data == nullptr)
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

err_flags __stack_ctor (stack_t *stk, size_t obj_size, size_t capacity, elem_print_f print_func)
{
    assert (obj_size > 0 && "object size cant be 0");

    #if DUNGEON_MASTER_PROTECT
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

    if (print_func != nullptr)  stk->print_func = print_func;
    else                        stk->print_func = byte_fprintf;

    #if POISON_PROTECT
        memset (stk->data, POISON_BYTE, capacity*obj_size);
    #endif

    #if DUNGEON_MASTER_PROTECT
        *(dungeon_master_t*)stk->data = dungeon_master_val;
        stk->data = (dungeon_master_t*)stk->data + 1;
        * (dungeon_master_t *) ((char *)stk->data + stk->capacity * stk->obj_size) = dungeon_master_val;
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

    #if DUNGEON_MASTER_PROTECT
        stk->data = ((dungeon_master_t*) stk->data) - 1;
    #endif

    #if DUNGEON_MASTER_PROTECT
        void *new_data_ptr = realloc (stk->data, new_capacity*stk->obj_size + 2*sizeof (dungeon_master_t)); // Не заполняет нулями
    #else
        void *new_data_ptr = realloc (stk->data, new_capacity*stk->obj_size); // Не заполняет нулями
    #endif

    if (new_data_ptr == nullptr) return res::NOMEM;

    #if DUNGEON_MASTER_PROTECT
        new_data_ptr = ((dungeon_master_t*) new_data_ptr) + 1;
        * ((dungeon_master_t *) ((char *)new_data_ptr + new_capacity * stk->obj_size)) = dungeon_master_val;
    #endif

    #if POISON_PROTECT
        if (new_capacity > stk->capacity)
        {
            memset ((char* ) new_data_ptr + stk->capacity*stk->obj_size, POISON_BYTE, (new_capacity - stk->capacity)*stk->obj_size);
        }
    #endif

    stk->data     = new_data_ptr;
    stk->capacity = new_capacity;

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

    #if POISON_PROTECT
        memset ((char* ) stk->data + stk->size*stk->obj_size, POISON_BYTE, stk->obj_size);
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

    #if DUNGEON_MASTER_PROTECT
        stk->data = ((dungeon_master_t*) stk->data) - 1;
    #endif

    #if POISON_PROTECT
        memset ((char* ) stk->data, POISON_BYTE, stk->obj_size * stk->capacity);
    #endif

    free (stk->data);

    #if POISON_PROTECT
        // Poisoning
        stk->data     = const_cast<void *>(POISON_PTR); // Write to POISON_PTR is SegFault, but this is main idea of poisoning
        stk->size     = -1u;
        stk->capacity =   0;
        stk->obj_size =   0;
        stk->reserved = -1u;
    #endif

    return res::OK;
}

void stack_dump (const stack_t *stk, FILE *stream)
{
    fprintf (stream, R Bold "\n======== STACK DUMP =======\n" Plain D);

    if (stk == nullptr)
    {
        fprintf (stream, "Stack ptr is nullptr\n");
        return;
    }
    if (stack_verify (stk) & res::POISONED)
    {
        fprintf (stream, "Stack " R "POISONED" D "\n");
        return;
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
        stk->print_func ((char *) stk->data + i*stk->obj_size, stk->obj_size, stream);
    }

    fprintf (stream, R Bold "======== END STACK DUMP =======\n\n" Plain D);
}

void stack_perror (err_flags errors, FILE *stream, const char *prefix)
{
    if (errors & res::NULLPTR)
        fprintf (stream, "%sStack pointer is nullptr\n", prefix ? prefix : "");

    if (errors & res::OVER_FILLED)
        fprintf (stream, "%sStack is overfilled (size > capacity)\n", prefix ? prefix : "");

    if (errors & res::POISONED)
        fprintf (stream, "%sStack is posioned (posible use after free)\n", prefix ? prefix : "");

    if (errors & res::NOMEM)
        fprintf (stream, "%sOut of Memory\n", prefix ? prefix : "");

    if (errors & res::EMPTY)
        fprintf (stream, "%sPop from emty stack\n", prefix ? prefix : "");

    if (errors & res::BAD_CAPACITY)
        fprintf (stream, "%sBad stack capacity (< reserved, or != 0 with null data)\n", prefix ? prefix : "");

    if (errors & DATA_CORRUPTED)
        fprintf (stream, "%sStack data is corrupted\n", prefix ? prefix : "");
}

void byte_fprintf (void *elem, size_t elem_size, FILE *stream)
{
    #if POISON_PROTECT
        bool is_poison = true;
    #else
        bool is_poison = false;
    #endif

    for (size_t j = 0; j < elem_size; ++j)
    {
        
        #if POISON_PROTECT
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