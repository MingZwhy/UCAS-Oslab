#include <pthread.h>

/* TODO:[P4-task4] pthread_create/wait */
void pthread_create(pthread_t *thread,
                   void (*start_routine)(void*),
                   void *arg)
{
    /* TODO: [p4-task4] implement pthread_create */
    char *argv[2];
    argv[0][0] = arg;
    argv[0][1] = '\0';
    sys_pthread_create(thread, start_routine, argv);
    sys_yield();
}

int pthread_join(pthread_t thread)
{
    /* TODO: [p4-task4] implement pthread_join */
    sys_waitpid(thread);
    return 0;
}
