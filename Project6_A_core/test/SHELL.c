/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>


#define SHELL_BEGIN 20
#define UCAS 21
#define INPUT_LOC 15
#define CMD_NUM 17
#define COMMAND_MAXLEN 32

#define MAX_BUFF_SIZE 100
#define MAX_NUM_OF_ARGV 10

#define bool int
#define true 1
#define false 0
#define NULL 0

#define MAX_COMMAND_NUM 50

//ascall
#define DEL1 8
#define DEL2 127
static char blank[] = {"                                                                   "};

typedef struct SHELL
{
    char name[COMMAND_MAXLEN];
    void (*func)(int argc, char *argv[]);
}SHELL_t, *ptr_to_SHELL_t;

SHELL_t Shell_Command[MAX_COMMAND_NUM];

void Init_Shell_Command()
{
    strcpy(Shell_Command[0].name, "ps");
    Shell_Command[0].func = &cmd_ps;

    strcpy(Shell_Command[1].name, "cl");
    Shell_Command[1].func = &cmd_clear;

    strcpy(Shell_Command[2].name, "exec");
    Shell_Command[2].func = &cmd_exec;

    strcpy(Shell_Command[3].name, "kill");
    Shell_Command[3].func = &cmd_kill;

    strcpy(Shell_Command[4].name, "taskset");
    Shell_Command[4].func = &cmd_taskset;

    strcpy(Shell_Command[5].name, "mkfs");
    Shell_Command[5].func = &cmd_mkfs;

    strcpy(Shell_Command[6].name, "ls");
    Shell_Command[6].func = &cmd_ls;

    strcpy(Shell_Command[7].name, "mkdir");
    Shell_Command[7].func = &cmd_mkdir;

    strcpy(Shell_Command[8].name, "rmdir");
    Shell_Command[8].func = &cmd_rmdir;

    strcpy(Shell_Command[9].name, "cd");
    Shell_Command[9].func = &cmd_cd;

    strcpy(Shell_Command[10].name, "statfs");
    Shell_Command[10].func = &cmd_statfs;

    strcpy(Shell_Command[11].name, "touch");
    Shell_Command[11].func = &cmd_touch;

    strcpy(Shell_Command[12].name, "cat");
    Shell_Command[12].func = &cmd_cat;

    strcpy(Shell_Command[13].name, "ln");
    Shell_Command[13].func = &cmd_ln;

    strcpy(Shell_Command[14].name, "rm");
    Shell_Command[14].func = &cmd_rm;

    strcpy(Shell_Command[15].name, "clearSD");
    Shell_Command[15].func = &cmd_clearSD;

    strcpy(Shell_Command[16].name, "waitpid");
    Shell_Command[16].func = &cmd_waitpid;
}

void cmd_ps(int argc, char *argv[]);
void cmd_clear(int argc, char *argv[]);
pid_t cmd_exec(int argc, char *argv[]);
int cmd_kill(int argc, char *argv[]);
int cmd_waitpid(int argc, char *argv[]);
void cmd_taskset(int argc, char *argv[]);
void cmd_clearSD(int argc, char*argv[]);
void cmd_rm(int argc,char *argv[]);
void cmd_ln(int argc,char*argv[]);
void cmd_mkfs(int argc, char*argv[]);
void cmd_ls(int argc,char*argv[]);
void cmd_mkdir(int argc, char*argv[]);
void cmd_rmdir(int argc, char*argv[]);
void cmd_cd(int argc,char*argv[]);
void cmd_statfs(int argc, char*argv[]);
void cmd_touch(int argc, char*argv[]);
void cmd_cat(int argc,char*argv[]);

void cmd_ps(int argc, char *argv[])
{
    sys_ps();
}

void cmd_clear(int argc, char *argv[])
{
    sys_clear();
    sys_move_cursor(0, SHELL_BEGIN);
    
    printf("------------------- COMMAND -------------------\n");
    //printf("> root@UCAS_OS: ");
}


pid_t cmd_exec(int argc, char *argv[])
{
    if( argc < 2 )
    {
        printf("ERROR: too few param!\n");
        return 0;
    }
    //sys_exec(char *name, int argc, char **argv)
    char *name = argv[1];

    //#define NULL 0
    bool need_wait = true;
    if(argv[argc-1] != NULL)
    {
        need_wait = (strcmp(argv[argc-1], "&") != 0);
        if(need_wait == false)
        {
            argc--;
        }
    }

    int r_argc = argc - 1;
    char **r_argv = argv + 1;
    
    int p = sys_exec(name, r_argc, r_argv);
    printf("Info: execute %s successfully, pid = %d\n", name, p);
    
    /*
    if(p == 0)
    {
        //printf("reject! process has been running!\n");
        return 0;
    }
    if(need_wait)
    {
        //printf("Info: execute successfully, pid = %d, waiting...\n", p);
        sys_waitpid(p);
    }
    else
    {
        printf("Info: execute %s successfully, pid = %d\n", name, p);
    }
    */
    
    return p;
}


int cmd_kill(int argc, char *argv[])
{
    char *str_pid = argv[1];
    int len = strlen(str_pid);
    pid_t pid = 0;
    int index = 0;
    while(str_pid[index]!='\0')
    {
        pid = (pid * 10) + (str_pid[index] - '0');
        index++;
    }
    
    int r =  sys_kill(pid);
    if(r == 0)
    {
        printf("fail to find pid %d\n",pid);
    }
    else
    {
        printf("kill pid %d successfully\n",pid);
    }
    return r;
}

int cmd_waitpid(int argc, char *argv[])
{
    char *str_pid = argv[1];
    int len = strlen(str_pid);
    pid_t pid = 0;
    int index = 0;
    while(str_pid[index]!='\0')
    {
        pid = (pid * 10) + (str_pid[index] - '0');
        index++;
    }

    int r = sys_waitpid(pid);
    if(r == 0)
    {
        printf("pid %d is not running\n",pid);
    }
    else
    {
        printf("now it's waiting pid %d\n",r);
    }
    return r;
}

void cmd_taskset(int argc, char *argv[])
{
    sys_taskset_exec(argc, argv);
}

void cmd_clearSD(int argc, char*argv[])
{
    printf("begin to clear file_system in SD (it will take about 10s)\n");
    sys_clearSD();
    printf("clear successfully!\n");
}

void cmd_rm(int argc,char *argv[])
{
    char *name = argv[1];
    if(-1 == sys_rm(name))
    {
        printf("ERROR: Wrong Path Name!\n");
    }
}

void cmd_ln(int argc,char*argv[])
{
    if(-1 == sys_ln(argv[1],argv[2]))
    {
        printf("ERROR: Wrong Path Name!\n");
    }
}

void cmd_mkfs(int argc, char*argv[])
{
    sys_mkfs();
}

void cmd_ls(int argc,char*argv[])
{
    int option = strcmp(argv[1],"-l") == 0; 
    sys_ls(argv[1],option);
}

void cmd_mkdir(int argc, char*argv[])
{
    char *name = argv[1];
    if(-1 == sys_mkdir(name))
    {
        printf("ERROR: Same Path Name!\n");
    }
}

void cmd_rmdir(int argc, char*argv[])
{
    char *name = argv[1];
    if(-1 == sys_rmdir(name))
    {
        printf("ERROR: No That Path!\n");
    }
}

void cmd_cd(int argc,char*argv[])
{
    char *name = argv[1];
    if(-1 == sys_cd(name))
    {
        printf("ERROR: Wrong Path Name!\n");
    }
}

void cmd_statfs(int argc, char*argv[])
{
    sys_statfs();
}

void cmd_touch(int argc, char*argv[])
{
    char *name = argv[1];
    if(-1 == sys_touch(name))
    {
        printf("ERROR: Same Path Name!\n");
    }
}

void cmd_cat(int argc,char*argv[])
{
    char *name = argv[1];
    if(-1 == sys_cat(name))
    {
        printf("ERROR: Wrong Path Name!\n");
    }
}

int handle_input(char *buff, int buff_size)
{
    printf("$");
    memset(buff, 0, buff_size);
    char *end = buff;
    
    for(;;)
    {
        int ch = -1;
        while(ch == -1)
        {
            ch = sys_getchar();
        }
        if(ch > 0)
        {
            if(ch == '\r' || ch == '\n')         //换行
            {
                printf("\n");
                break;
            }
            else if(ch == DEL1 || ch == DEL2)    //删除回退
            {
                if(end != buff)   //prevent it delete '$'
                {
                    printf("\b");
                    end--;
                }
            }
            else
            {
                printf("%c", ch);
                *end = ch;
                end++;
            }
        }
        else
        {
            //the end is '\0'
            *end = ch;
            end++;
            break;
        }
    }

    //return size
    int size = end - buff;
    return size;
}

void handle_argc(char *buff, int buff_size, int *argc, char *argv[])
{
    *argc = 0;

    bool input_next = true;

    for(int i=0; i<buff_size && buff[i]!='\0'; i++)
    {
        if(buff[i] == ' ')
        {
            input_next = true;
            buff[i] = '\0';
        }
        else
        {
            if(input_next)
            {
                argv[(*argc)] = buff + i;
                (*argc)++;
                input_next = false;
            }
        }
    }
    return;
}

void handle_argv(int argc, char *argv[])
{
    for (int i = 0; i < CMD_NUM; i++)
    {
        if(strcmp(Shell_Command[i].name, argv[0]) == 0)
        {
            Shell_Command[i].func(argc,argv);
            return;
        }
    }
    printf("wrong command, please input again!\n");
}

int main(void)
{
    int print_location;
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    sys_move_cursor(0, UCAS);
    printf("> root@UCAS_OS: ");
    sys_move_cursor(INPUT_LOC, UCAS);

    char buff[MAX_BUFF_SIZE];
    int argc;
    char *argv[MAX_NUM_OF_ARGV];

    Init_Shell_Command();
    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)
        if(handle_input(buff, sizeof(buff)) <= 0)
        {
            continue;
        }
        handle_argc(buff, sizeof(buff), &argc, argv);
        handle_argv(argc, argv);

        printf("> root@UCAS_OS:");

        // TODO [P3-task1]: ps, exec, kill, clear        
    }

    return 0;
}
