#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "stack.h"

unsigned int stack_verify (const stack_t *stk)
{
    unsigned int ret = res::OK;

    if (stk == nullptr) { return res::NULLPTR; }
    if (stk->data == POISON_PTR) { ret |= res::POISONED; }
    if (stk->data == nullptr && stk->size != 0 && stk->capacity != 0)
        { ret |= res::OVER_FILLED; }
    if (stk->size > stk->capacity) { ret |= res::OVER_FILLED; }
    if (stk->capacity < stk->reserved) { ret |= res::BAD_CAPACITY; }
    if (stk->obj_size == 0) { ret |= res::POISONED; };

    return ret;
}

res __stack_ctor (stack_t *stk, size_t obj_size, size_t capacity)
{
    assert (obj_size > 0 && "object size cant be 0");

    #ifndef NDEBUG
        if (stk->data != nullptr) { log (log::DBG, "Allocating stack data with not nullptr data pointer"); }
    #endif

    void *mem_ptr = calloc (capacity, obj_size); // Works even with capacity = 0
    if (mem_ptr ==  nullptr) { return res::NOMEM; }

    stk->data     = mem_ptr;
    stk->capacity = capacity;
    stk->reserved = capacity;
    stk->size = 0;
    stk->obj_size = obj_size;

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

    stk->data     = new_data_ptr;
    stk->capacity = new_capacity;

    stack_assert (stk);
    return res::OK;
}

res shrink_to_fit (stack_t *stk)
{
    stack_assert (stk);

    stack_resize (stk, stk->size);

    stack_assert (stk);
    return res::OK;
}

res pop (stack_t *stk, void *value)
{
    stack_assert (stk);
    assert (value != nullptr && "pointer can't be NULL");

    if (stk->size == 0)
    {
        return res::EMPTY;
    }
    stk->size--;
    memcpy (value, (char* ) stk->data + stk->size*stk->obj_size, stk->obj_size);

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

res push (stack_t *stk, const void *value)
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
    fprintf (stream, "======== STACK DUMP =======\n");

    if (stk == nullptr)
    {
        fprintf (stream, "Stack ptr is nullptr\n");
        return;
    }
    if (stack_verify (stk) & res::POISONED)
    {
        fprintf (stream, "Stack POISONED\n");
        return;
    }

    #ifndef NDEBUG
        fprintf (stream, "Stack[%p] with name %s allocated at %s at file %s:(%u)\n",
            stk, stk->debug_data->var_name, stk->debug_data->func_name, stk->debug_data->file, stk->debug_data->line
        );
    #else
        fprintf (stream, "Stack[%p]\n", stk);
    #endif

    fprintf (stream, "Parameters: size: %lu capacity: %lu object size: %lu reserved size: %lu\n", stk->size, stk->capacity, stk->obj_size, stk->reserved);
    fprintf (stream, "Stack data[%p]\n", stk->data);
    for (size_t i = 0; i < stk->capacity; ++i)
    {
        if (i<stk->size) fprintf (stream, "* ");
        else             fprintf (stream, "  ");

        fprintf (stream, "data[%lu]: ", i);
        for (size_t j = 0; j < stk->obj_size; ++j)
        {
            fprintf (stream, "|0x%08x|", ((unsigned char *)stk->data)[stk->obj_size*i+j]);
        }
        fprintf (stream, "\n");
    }
}