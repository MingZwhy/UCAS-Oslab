#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdatomic.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>

#define SNAP_KEY 26
int main(int argc, char *argv[])
{
    srand(clock());
    long data = 1;
    long pi [12]  = {3,14,15,92,65,35,89,79,32,38,46,26}; 
    int top = 0;
    uintptr_t address0 = sys_snapget(SNAP_KEY);
    int mode = 0;
    for (int i = 0; i < 10; i++)
    {
        uintptr_t address1 = sys_snapget(SNAP_KEY);
        int mode = rand() % 3;
        if (mode != 0)
        {
            *(long *)address1 = pi[top++];
        }
        sys_sleep(1);
        printf("round: %d  va: %x  pa: %x  data: %d\n", i, address1, sys_srchpa(address1), *(long *)address1);
    }
    printf("Success!");
    return;
}