#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdatomic.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>

const int key = 5;
int main(int argc, char *argv[])
{
    srand(clock());
    long data = 1;
    long pi [12]  = {3,14,15,92,65,35,89,79,32,38,46,26}; 
    int top = 0;
    uintptr_t origin_addr = sys_shmpageget(key);
    int mode = 0;
    printf("\nbegin\n");
    for (int i = 0; i < 10; i++)
    {
        //创建快照
        uintptr_t address1 = sys_shmpageget(key);
        //printf("get page %x\n", address1);
        int mode = rand() % 3;
        if (mode != 0)
        {
            //修改正本
            *(long *)origin_addr = pi[top++];
        }
        else
        {
            //不修改正本
        }
        //printf("after write\n");
        sys_sleep(1);
        printf("round: %d  va: %x  pa: %x  data: %d\n", i, address1, sys_get_pageaddr(address1), *(long *)address1);
    }
    printf("Success!");
    return;
}