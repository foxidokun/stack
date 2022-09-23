#include "stack.h"
#include "log.h"
#include "test.h"

int main ()
{
    set_log_level (log::DBG);
    set_log_stream (stdout);

    run_tests();
}