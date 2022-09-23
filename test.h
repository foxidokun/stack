#ifndef TEST_H
#define TEST_H

/**
 * @return     Non-zero value on error
 */
int test_stack_ctor_notinit ();
int test_stack_ctor_init ();
int test_stack_push_pop_no_resize ();
int test_stack_push_pop_manual_realloc ();
int test_stack_push_pop_auto_realloc ();
int test_stack_push_pop_auto_shrink ();

void run_tests ();


#endif