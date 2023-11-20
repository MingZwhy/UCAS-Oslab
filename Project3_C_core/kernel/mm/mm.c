#include <os/mm.h>

static ptr_t kernMemCurr = FREEMEM_KERNEL;
static ptr_t userMemCurr = FREEMEM_USER;

int kernel_page_valid[KERNEL_PAGE_MAXNUM];
int user_page_valid[USER_PAGE_MAXNUM];

void init_page_valid(void)
{
    for(int i=0; i<KERNEL_PAGE_MAXNUM; i++)
    {
        kernel_page_valid[i] = 0;
    }

    for(int i=0; i<USER_PAGE_MAXNUM; i++)
    {
        user_page_valid[i] = 0;
    }
}

ptr_t allocKernelPage(int numPage, int *index)
{
    // align PAGE_SIZE
    for(int i=0; i<KERNEL_PAGE_MAXNUM; i++)
    {
        if(kernel_page_valid[i] == 0)
        {
            *index = i;
            kernel_page_valid[i] = 1;
            break;
        }
    }

    ptr_t addr = kernMemCurr + (*index) * PAGE_SIZE;
    addr = ROUND(addr, PAGE_SIZE);
    return addr;
}

ptr_t allocUserPage(int numPage, int *index)
{
    // align PAGE_SIZE
    for(int i=0; i<USER_PAGE_MAXNUM; i++)
    {
        if(user_page_valid[i] == 0)
        {
            *index = i;
            user_page_valid[i] = 1;
            break;
        }
    }

    ptr_t addr = userMemCurr + (*index) * PAGE_SIZE;
    addr = ROUND(addr, PAGE_SIZE);
    return addr;
}

void recovKernelPage(int numPage, int index)
{
    if(index >=0 || index < KERNEL_PAGE_MAXNUM)
    {
        kernel_page_valid[index] = 0;
    }
}

void recovUserpage(int numPage, int index)
{
    if(index >=0 || index < USER_PAGE_MAXNUM)
    {
        user_page_valid[index] = 0;
    }
}
