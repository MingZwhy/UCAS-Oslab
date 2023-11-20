#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#define TEST_TIME 20
#define TEST_LEN 50
#define PAGE_SIZE 0x10000

int main(int argc, char* argv[])
{
    printf("test begin!\n");
    srand(clock());
    long data = 0;
    uintptr_t addr = 0;
    uintptr_t addr_base = 0x10800000;
    int test_time = TEST_TIME;

    for(int i=0; i<TEST_TIME; i++)
    {
        for(int j=0; j<TEST_LEN; j++)
        {
            addr = addr_base + PAGE_SIZE * j;
            data = rand();
            *(long *)addr = data;
            //printf("0x%lx, %ld\n", addr, data);
            if( *(long*)addr != data)
            {
                printf("Error!\n");
            }
        }
        if( i % 2 == 1 )
            printf("pass test %d/%d\n", i+1, test_time);
        else
            printf("pass test %d/%d         ", i+1, test_time);
    }

    printf("Success!\n");
}