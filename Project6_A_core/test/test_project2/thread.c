#include <stdio.h>
#include <assert.h>
#include <unistd.h>  // NOTE: use this header after implementing syscall!
#include <kernel.h>
#define MAXNUM 1001
#define A_core 1

int num[MAXNUM];
int sum;
short odd_ready = 0;
short even_ready = 0;

void odd_sum(){
    int print_location = 8;
    for(int i = 1;i<MAXNUM;i+=2)
    {
        sys_move_cursor(0, print_location);
        sum += num[i];
        printf("> [TASK] --Thread2 add (%d)", num[i]);
    }
    odd_ready = 1;
    while (1);    
}
void even_sum(){
    int print_location = 7;
    for(int i = 0;i<MAXNUM;i+=2)
    {
        sys_move_cursor(0, print_location);
        sum += num[i];
        printf("> [TASK] --Thread1 add (%d)", num[i]);
    }
    even_ready = 1;
    while (1);    
}

int main(void)
{
    int print_location = 6;
    for(int i=0;i<MAXNUM;i++){
        num[i] = i;
    }
    sys_thread_create(even_sum,1);
    sys_thread_create(odd_sum,2);
    
    while (!(even_ready&&odd_ready));

    sys_move_cursor(0, print_location);
    printf("> [TASK] Add result is (%d)", sum);
    
    while (1);

    return 0;
}
