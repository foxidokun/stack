#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "stack.h"

unsigned int stack_verify (const stack_t *stk)
{
    unsigned int ret = res::OK;

    if (stk == nullptr) { return res::NULLPTR; }

    if (stk->size > stk->capacity) { ret |= res::OVER_FILLED; }
    if (stk->capacity < stk->reserved) { ret |= res::BAD_CAPACITY; }
    if (stk->obj_size == 0) { ret |= res::POISONED; };

    if (stk->data == POISON_PTR) { ret |= res::POISONED; }
    if (stk->data == nullptr && (stk->size != 0 || stk->capacity != 0))
        { ret |= res::OVER_FILLED | BAD_CAPACITY; }

    if (!(ret & POISONED))
    {
        for (size_t n = stk->size; n < stk->capacity; ++n)
        {
            for (size_t i = 0; i < stk->obj_size; ++i)
            {
                if (((unsigned char *)stk->data)[stk->obj_size*n+i] != POISON_BYTE)
                {
                    ret |= res::CORRUPTED;
                }
            }
        }

        bool all_poisoned = (stk->size);

        for (size_t n = 0; n < stk->size; ++n)
        {
            for (size_t i = 0; i < stk->obj_size; ++i)
            {
                if (((unsigned char *)stk->data)[stk->obj_size*n+i] != POISON_BYTE)
                {
                    all_poisoned = false;
                }
            }
        }

        if (all_poisoned) ret |= POISONED;
    }

    return ret;
}

res __stack_ctor (stack_t *stk, size_t obj_size, size_t capacity)
{
    assert (obj_size > 0 && "object size cant be 0");

    void *mem_ptr = calloc (capacity, obj_size); // Works even with capacity = 0
    if (mem_ptr ==  nullptr) { return res::NOMEM; }

    stk->data     = mem_ptr;
    stk->capacity = capacity;
    stk->reserved = capacity;
    stk->size     = 0;
    stk->obj_size = obj_size;

    #ifndef NDEBUG
    memset (stk->data, POISON_BYTE, capacity*obj_size);
    #endif

    stack_assert (stk);

    return res::OK;
}

#ifndef NDEBUG
res __stack_ctor_with_debug (stack_t *stk, const stack_debug_t *debug_data,
                                size_t obj_size, size_t capacity)
{
    assert (stk != nullptr && "pointer can't be NULL");

    stk->debug_data = debug_data;

    return __stack_ctor (stk, obj_size, capacity);
}
#endif

res stack_resize (stack_t *stk, size_t new_capacity)
{
    stack_assert (stk);
    assert (stk->size <= new_capacity);

    void *new_data_ptr = realloc (stk->data, new_capacity*stk->obj_size); // Не заполняет нулями
    if (new_data_ptr == nullptr) return res::NOMEM;

    #ifndef NDEBUG
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

res stack_shrink_to_fit (stack_t *stk)
{
    stack_assert (stk);

    stack_resize (stk, stk->size);

    stack_assert (stk);
    return res::OK;
}

res stack_pop (stack_t *stk, void *value)
{
    stack_assert (stk);
    assert (value != nullptr && "pointer can't be NULL");

    if (stk->size == 0)
    {
        return res::EMPTY;
    }
    stk->size--;
    memcpy (value, (char* ) stk->data + stk->size*stk->obj_size, stk->obj_size);

    #ifndef NDEBUG
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

res stack_push (stack_t *stk, const void *value)
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

res stack_dtor (stack_t *stk)
{
    if (stk == nullptr) { return res::OK; }

    #ifndef NDEBUG
    memset ((char* ) stk->data, POISON_BYTE, stk->obj_size * stk->capacity);
    #endif

    free (stk->data);

    #ifndef NDEBUG
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

    bool is_poison = true;

    for (size_t i = 0; i < stk->capacity; ++i)
    {
        is_poison = true;

        fprintf (stream, "%c data[%03lu]: ", (i<stk->size ? '*' : ' '), i);
        for (size_t j = 0; j < stk->obj_size; ++j)
        {
            if (((unsigned char *)stk->data)[stk->obj_size*i+j] != POISON_BYTE) 
            {
                is_poison = false;
            }

            fprintf (stream, "|0x%08x|", ((unsigned char *)stk->data)[stk->obj_size*i+j]);
        }

        if (is_poison)
        {
            fprintf (stream, (i < stk->size) ? R : D "  (POISON)" D); 
        }

        fprintf (stream, "\n");
    }

    fprintf (stream, R Bold "======== END STACK DUMP =======\n\n" Plain D);
}