#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "stack.h"

int stack_verify (const stack_t *stk)
{
    if (stk == nullptr) { return res::NULLPTR & res::OK; }
    if (stk->data == POISON_PTR) { return res::POISONED; }
    if (stk->data == nullptr && stk->size != 0 && stk->capacity != 0)
        { return res::OVER_FILLED; }
    if (stk->size > stk->capacity) { return res::OVER_FILLED; }
    if (stk->obj_size == 0) { return res::POISONED; };

    return res::OK;
}

res __stack_ctor (stack_t *stk, size_t obj_size, size_t capacity)
{
    stack_assert (stk);
    assert (obj_size > 0 && "object size cant be 0");

    #ifndef NDEBUG
        if (stk->data != nullptr) { log (log::DBG, "Allocating stack data with not nullptr data pointer"); }
    #endif

    void *mem_ptr = calloc (capacity, obj_size); // Works even with capacity = 0
    if (mem_ptr ==  nullptr) { return res::NOMEM; }

    stk->data     = mem_ptr;
    stk->capacity = capacity;
    stk->size = 0;
    stk->obj_size = obj_size;

    stack_assert (stk);

    return res::OK;
}

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

    if (stk->capacity>>2 > stk->size)
    {
        UNWRAP (stack_resize (stk, stk->capacity>>1));
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

    // Poisoning
    stk->size     = -1u;
    stk->capacity =   0;
    stk->obj_size =   0;

    return res::OK;
}

void stack_dump (const stack_t *stk, FILE *stream)
{
    fprintf (stream, "STACK DUMP\n");

    if (stk == nullptr)
    {
        fprintf (stream, "Stack \n");
        return;
    }

    #ifndef NDEBUG
        fprintf (stream, "Stack[%p] with name %s allocated at %s at file %s:(%d)",
            stk, stk->debug_data->var_name, stk->debug_data->func_name, stk->debug_data->file, stk->debug_data->line
        );
    #else
        fprintf (stream, "Stack[%p]", stk);
    #endif

    fprintf (stream, "Parameters: size: %lu capacity: %lu object size: %lu\n", stk->size, stk->capacity, stk->obj_size);
    fprintf (stream, "Stack data[%p]", stk->data);
    for (size_t i = 0; i < stk->capacity; ++i)
    {
        if (i<stk->size) fprintf (stream, "* ");
        else             fprintf (stream, "  ");

        fprintf (stream, "data[%p]: ", stk->data);
        for (size_t j = 0; j < stk->obj_size; ++j)
        {
            fprintf (stream, "|%08x|", ((unsigned char *)stk->data)[stk->obj_size*i+j]);
        }
        fprintf (stream, "\n");
    }
}