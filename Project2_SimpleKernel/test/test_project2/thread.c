#include<stdio.h>
#include<unistd.h>

#define MAX_LEN 3000
#define bool int
#define TRUE 1
#define FALSE 0
#define KEEP_WHILE 1

int num[MAX_LEN];
int sum = 0;
bool If_Thread1_Finish = FALSE;
bool If_Thread2_Finish = FALSE;
//考虑到前面已有5行打印
int print_location = 5;

void add_num1()
{
    for(int i = 0; i < MAX_LEN; i += 2)
    {
        sum += num[i];
        sys_move_cursor(0, print_location);
        printf("> [Self_TASK] thread one has added (%d)", num[i]);
    }

    //finish
    If_Thread1_Finish = TRUE;

    //keep while
    while(KEEP_WHILE);
}

void add_num2()
{
    for(int i = 1; i < MAX_LEN; i += 2)
    {
        sum += num[i];
        sys_move_cursor(0, print_location);
        printf("> [Self_TASK] thread two has added (%d)", num[i]);
    }

    //finish
    If_Thread2_Finish = TRUE;

    //keep while
    while(KEEP_WHILE);
}

int main()
{
    //initialize num[]
    for(int i = 0; i < MAX_LEN; i++)
    {
        num[i] = i;
    }

    //create_thread
    int pid1 = 1;
    sys_thread_create(add_num1, pid1);
    int pid2 = 2;
    sys_thread_create(add_num2, pid2);

    //wait two thread finish task
    while(!If_Thread1_Finish || !If_Thread2_Finish);

    sys_move_cursor(0, print_location);
    printf("> [Self_TASK] finish! 0 - 2999, sum result is (%d)", sum);

    //keep while
    while(KEEP_WHILE);
}