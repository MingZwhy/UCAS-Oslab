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
    long num [12]  = {3,14,15,92,65,35,89,79,32,38,46,26}; 
    int top = 0;
    uintptr_t origin_addr = sys_get_snap_page(key);
    int mode = 0;
    printf("\nbegin\n");
    for (int i = 0; i < 12; i++)
    {
        printf("round: %d", i);

        uintptr_t copy_addr;
        int mode = rand() % 3;
        if (mode != 0)
        {
            //创建快照并修改正本
            //预期效果：重新为正本分配了物理地址
            //此时正本和copy对应的物理地址和内容均不同
            copy_addr = sys_get_snap_page(key);
            *(long *)origin_addr = num[top++];
            printf("  change the origin:\n");
        }
        else
        {
            //创建快照但不修改正本
            //预期效果：正本与副本有相同物理地址且内容相同
            copy_addr = sys_get_snap_page(key);
            printf("  no change:\n");
        }

        sys_sleep(1);
        printf("origin --> va: %x  pa: %x  data: %d\n", origin_addr, sys_get_pageaddr(origin_addr), *(long *)origin_addr);
        printf("copy   --> va: %x  pa: %x  data: %d\n\n", copy_addr, sys_get_pageaddr(copy_addr), *(long *)copy_addr);
    }
    printf("Success!");
    return;
}