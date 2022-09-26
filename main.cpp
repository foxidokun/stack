#include "stack.h"
#include "log.h"
#include "test.h"

void int_printf (const void *elem, size_t elem_size, FILE *stream);

int main ()
{
    set_log_level (log::DBG);
    set_log_stream (stdout);

    stack_t stk = {};
    stack_ctor (&stk, sizeof (int), 16, int_printf);

    for (size_t i = 0; i < 8; ++i)
    {
        stack_push (&stk, &i);
    }

    stack_dump(&stk, stdout);

    stack_dtor (&stk);

    run_tests();
}

void int_printf (const void *elem, size_t elem_size, FILE *stream)
{
    fprintf (stream, "%d", *(const int *)elem);
}