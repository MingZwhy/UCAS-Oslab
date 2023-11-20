#include <common.h>
#include <asm.h>
#include <os/bios.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 128

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_bios(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())BIOS_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(short task_info_loc)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
	/*
	bios_putstr("\ninto init task_info\n");
	int temp = task_info_loc;
        while(temp>0){
            bios_putchar(temp%10 + '0');
            temp = temp / 10;
        }
	*/

    int real_info_loc = 0x52030000 + task_info_loc;


    task_info_t * src = real_info_loc;
    memcpy(tasks, src, sizeof(task_info_t)*4);

    /*
    for(int i=0; i<=3; i++)
    {
	memcpy(tasks[i].file_name, real_info_loc, 8);
	real_info_loc += 8;
	memcpy(&tasks[i].offset,real_info_loc,4);
	real_info_loc += 4;
	memcpy(&tasks[i].file_size,real_info_loc,4);
	real_info_loc += 4;
    }
    */
 

    for(int i=0; i<=3; i++){
        bios_putstr(tasks[i].file_name);
        bios_putstr("\noffset is:");
        int file_size = tasks[i].file_size;
        int offset = tasks[i].offset;
        while(offset>0){
            bios_putchar(offset%10 + '0');
            offset = offset / 10;
        }
        bios_putstr("\nsize is:");
        while(file_size>0){
            bios_putchar(file_size%10 + '0');
            file_size = file_size / 10;
        }
	bios_putchar('\n');
    }
}

int main(short task_info_loc)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by BIOS (ΦωΦ)
    init_bios();

    // Init task information (〃'▽'〃)
    init_task_info(task_info_loc);

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    
    unsigned mem_address;
    bios_putstr("batch processing (according to batch1.txt)\n\r");
    int batch_loc = 0x50200000 + 0x1fe - 13;
    char *loc = batch_loc;

    char process = '0';
    do{
	    memcpy(&process, loc, sizeof(char));
	    bios_putstr("copy content:");
	    bios_putchar(process);
	    loc++;
	    if(process == 'x')
		    break;
	    mem_address = load_task_img(process - '0');
	    void (*func)(void) = (void *)mem_address;
	    func();
    }while(1);


    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
