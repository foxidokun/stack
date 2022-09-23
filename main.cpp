#include "stack.h"
#include "log.h"
#include "test.h"

int main ()
{
    set_log_level (log::DBG);
    set_log_stream (stdout);

    stack_t stk = {};
    stack_ctor (&stk, sizeof (int), 16);

    for (size_t i = 0; i < 8; ++i)
    {
        stack_push (&stk, &i);
    }

    stack_dump(&stk, stdout);

    stack_dtor (&stk);

    run_tests();
}