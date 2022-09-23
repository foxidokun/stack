#include <stdio.h>
#include "stack.h"
#include "test.h"

#define R "\033[91m"
#define G "\033[92m"
#define D "\033[39m"

#define _TEST(cond)                                             \
{                                                               \
    if (cond)                                                   \
    {                                                           \
        printf (R "Test FAILED: %s\n\n" D, #cond);              \
        failed++;                                               \
    }                                                           \
    else                                                        \
    {                                                           \
        printf (G "Test OK:     %s\n\n" D, #cond);              \
        success++;                                              \
    }                                                           \
}

#define _ASSERT(cond)                                                           \
{                                                                               \
    if (!(cond))                                                                \
    {                                                                           \
        printf (R "## Test Error: %s##\n" D, __func__);                         \
        printf ("Condition check failed: %s\n", #cond);                         \
        printf ("Test location: File: %s Line: %d\n", __FILE__, __LINE__);      \
        return -1;                                                              \
    }                                                                           \
}

// ----- TESTS ------

int test_stack_ctor_notinit ()
{
    stack_t stk;
    stack_ctor (&stk, sizeof (int));

    _ASSERT (stk.size == 0);
    _ASSERT (stk.capacity == 0);
    _ASSERT (stk.obj_size == sizeof (int));

    stack_dtor (&stk);
    return 0;
}

int test_stack_ctor_init ()
{
    stack_t stk = {};
    stack_ctor (&stk, sizeof (int));

    _ASSERT (stk.size == 0);
    _ASSERT (stk.capacity == 0);
    _ASSERT (stk.obj_size == sizeof (int));

    stack_dtor (&stk);
    return 0;
}

int test_stack_push_pop_no_resize ()
{
    stack_t stk = {};
    stack_ctor (&stk, sizeof (int), 4);

    int a = 4, b = 7;

    _ASSERT (push (&stk, &a) == res::OK);
    _ASSERT (push (&stk, &b) == res::OK);
    _ASSERT (pop  (&stk, &a) == res::OK);
    _ASSERT (pop  (&stk, &b) == res::OK);

    _ASSERT (a == 7);
    _ASSERT (b == 4);

    stack_dtor (&stk);
    return 0;
}

int test_stack_push_pop_manual_realloc ()
{
    stack_t stk = {};
    stack_ctor (&stk, sizeof (int), 4);

    const int new_capacity = 8;

    _ASSERT (stack_resize(&stk, new_capacity) == res::OK);
    _ASSERT (stk.size == 0);
    _ASSERT (stk.capacity == new_capacity);

    for (int i = 0; i < new_capacity; ++i)
    {
        _ASSERT (push(&stk, &i) == res::OK);
    }

    _ASSERT (stk.size == new_capacity);
    _ASSERT (stk.capacity == new_capacity);

    int poped = 0;

    for (int i = new_capacity - 1; i >= 0; --i)
    {
        _ASSERT (pop(&stk, &poped) == res::OK);
        _ASSERT (poped == i);
    }

    stack_dtor (&stk);
    return 0;
}

int test_stack_push_pop_auto_realloc ()
{
    stack_t stk = {};
    stack_ctor (&stk, sizeof (int), 4);

    const int new_capacity = 228;

    for (int i = 0; i < new_capacity; ++i)
    {
        _ASSERT (push(&stk, &i) == res::OK);
    }

    _ASSERT (stk.capacity == 256);
    int poped = 0;

    for (int i = new_capacity - 1; i >= 0; --i)
    {
        _ASSERT (pop(&stk, &poped) == res::OK);
        _ASSERT (poped == i);
    }

    stack_dtor (&stk);
    return 0;
}

int test_stack_push_pop_auto_shrink ()
{
    stack_t stk = {};
    stack_ctor (&stk, sizeof (int), 16);

    const int new_capacity = 8192;

    int tmp = 0;

    for (size_t i = 0; i < new_capacity; ++i)
    {
        _ASSERT (push (&stk, &tmp) == res::OK);
    }

    _ASSERT (stk.size == new_capacity);
    _ASSERT (stk.capacity == new_capacity);

    for (size_t i = 8; i < new_capacity; ++i)
    {
        _ASSERT (pop (&stk, &tmp) == res::OK);
    }

    _ASSERT (stk.capacity == 16);
    _ASSERT (stk.size == 8);

    for (int i = 0; i < 8; ++i)
    {
        _ASSERT (pop (&stk, &tmp) == res::OK);
    }

    _ASSERT (stk.capacity == 16);
    _ASSERT (stk.size == 0);

    stack_dtor (&stk);
    return 0;
}


// ----- TEST LOGIC -----

void run_tests ()
{
    unsigned int success = 0;
    unsigned int failed  = 0;

    printf ("Starting tests...\n\n");

    _TEST (test_stack_ctor_notinit ());
    _TEST (test_stack_ctor_init ());
    _TEST (test_stack_push_pop_no_resize ());
    _TEST (test_stack_push_pop_manual_realloc ());
    _TEST (test_stack_push_pop_auto_realloc ());
    _TEST (test_stack_push_pop_auto_shrink ());

    printf ("Tests total: %u, failed %u, success: %u, success ratio: %3.1lf%%\n",
        failed + success, failed, success, success * 100.0 / (success + failed));
}