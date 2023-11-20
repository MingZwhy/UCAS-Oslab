#include <os/string.h>
#include <os/fs.h>
#include <assert.h>
#include <os/sched.h>
#include <pgtable.h>
#include <os/kernel.h>

static superblock_t superblock;
static fdesc_t fdesc_array[NUM_FDESCS];
static dentry_t tmp_dentry[NUM_DENTRY];
static uint64_t global_addr;
static inode_t tmp_inode;
static int currentfile;
static int basefile;

// 有关temp的addr分配函数
static void init_addr();              // addr函数初始化
static uint64_t alloc_addr();         // 分配一个暂存区
static void free_addr(uint64_t addr); // 释放暂存区

// 有关dir的函数
static void init_dir(int inode_index);
static void write_dir(int inode_index, int child_dir, char *child_name);
static int search_dir(char *path);
static int enter_dir(char *path);
static int remove_dir(char *path);

// 有关alloc的分配函数
int alloc_inode_helper(int imode, int mode, int size);
int alloc_map(uint64_t addr, int type);
int alloc_inode();
int alloc_data();

// 有关文件的函数
static int read_file(char *buff, int size, int inode_index, int offset);
static int write_file(char *buff, int size, int inode_index, int offset);
static int is_file(int ino);
static int remove_file(char *path);

// 有关inode的函数
static uint64_t rw_inode(int ino, uint64_t addr);
static int get_inode_block(int ino);

static int loc_datablock(int size);

static int addr_valid[MAX_ADDR_OF_ALLOC];
int do_mkfs(void)
{
    // TODO [P6-task1]: Implement do_mkfs
    // 首先初始化addr函数,以便后续分配地址
    printk("[FS]: Start initialize filesystem!\n");

    init_addr();
    printk("[FS]: Finish address setting...\n");

    uint64_t tmp_addr = alloc_addr();
    bios_sdread(kva2pa(tmp_addr), 1, START_FS);
    superblock_t *ptr_superblock = (superblock_t *)tmp_addr;
    if (SUPERBLOCK_MAGIC == ptr_superblock->magic_number)
    {
        printk("[FS]: Find filesystem in SD!\n");
        memcpy(&superblock, tmp_addr, sizeof(superblock_t));
        free_addr(tmp_addr);
        printk("[FS]: Initialize filesystem finished!\n");
    }
    else
    {
        free_addr(tmp_addr);

        printk("[FS]: Find no filesystem in SD...\n");
        printk("[FS]: Setting superblock...\n");

        // set superblock
        superblock.magic_number = SUPERBLOCK_MAGIC;
        printk("    :magic : 0x%x\n", superblock.magic_number);

        superblock.size = 0x20000000; // 文件系统为512MB
        printk("    :size : 0x%dB\n", superblock.size);
        // 这里采用block map，一共需要四个数据块进行存储
        superblock.startaddr_blockmap = START_FS + 1;
        superblock.num_of_blockmap = NUM_FS_BLOCK_MAP;
        printk("    :start block : 0x%d\n", superblock.startaddr_blockmap);
        printk("    :num   block : %d\n", superblock.num_of_blockmap);

        // 对于inode map，我们需要根据inode的大小设定，这里我们设定inode一共有2k项，那么inode map恰好1个扇区
        superblock.startaddr_inodemap = START_FS + 33;
        superblock.num_of_inodemap = NUM_FS_INODE_MAP;

        // 设定对应的inode
        superblock.startaddr_inodes = START_FS_INODE;
        superblock.num_of_inodes = 2048;

        // 剩下的都是data区域
        superblock.startaddr_data = START_FS_DATA;
        superblock.num_of_data = 0x100000 - 34 - 2048;

        // set origin fs
        currentfile = alloc_inode_helper(0, 0, 512);
        basefile = currentfile;
        init_dir(basefile);
        write_dir(basefile, basefile, ".");
        write_dir(basefile, basefile, "..");

        bios_sdwrite(kva2pa(&superblock), 1, START_FS);
        printk("[FS]: Initialize filesystem finished!\n");
    }
    return 0; // do_mkfs succeeds
}

int do_statfs(void)
{
    // TODO [P6-task1]: Implement do_statfs
    printk("    :magic : 0x%x\n", superblock.magic_number);
    printk("    :start block : 0x%d\n", superblock.startaddr_blockmap);
    printk("    :num   block : %d\n", superblock.num_of_blockmap);
    int sum = 0;
    for (int i = 0; i < MAX_ADDR_OF_ALLOC; i++)
    {
        sum += addr_valid[i];
    }
    printk("    :used alloc addr : %d/%d\n", sum, MAX_ADDR_OF_ALLOC);
    return 0; // do_statfs succeeds
}

int do_cd(char *path)
{
    // TODO [P6-task1]: Implement do_cd
    uint64_t str_addr[16] = {0};
    int j = 0;
    for (int i = 0;; i++)
    {
        if (path[j] == 0)
            break;
        str_addr[i] = path + j;
        while (1)
        {
            if (path[j] == 0 || path[j] == '/')
            {
                break;
            }
            j++;
        }
        if (path[j] == 0)
        {
            break;
        }
        else if (path[j] == '/')
        {
            path[j] = 0;
            j++;
        }
    }
    for (int i = 0; str_addr[i] != 0; i++)
    {
        if (-1 == enter_dir((char *)str_addr[i]))
        {
            return -1;
        }
    }
    // if(-1 == enter_dir(path))
    // {
    //     return -1;
    // }
    return 0; // do_cd succeeds
}

int do_mkdir(char *path)
{
    // TODO [P6-task1]: Implement do_mkdir
    if (-1 == search_dir(path))
    {
        int newfile = alloc_inode_helper(0, 0, 512);
        init_dir(newfile);
        write_dir(newfile, newfile, ".");
        write_dir(newfile, currentfile, "..");
        write_dir(currentfile, newfile, path);
    }
    else
    {
        return -1;
    }
    return 0; // do_mkdir succeeds
}

int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir
    if (0 == strcmp(path, ".") || 0 == strcmp(path, ".."))
        return -1;
    return remove_dir(path); // do_rmdir succeeds
}

int do_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    uint64_t tmp_addr = alloc_addr();
    uint64_t ptr_addr = rw_inode(currentfile, tmp_addr);
    inode_t *ptr_inode = (inode_t *)ptr_addr;
    int loc = ptr_inode->dir_blocks[0];
    free_addr(tmp_addr);
    tmp_addr = alloc_addr();
    bios_sdread(kva2pa(tmp_addr), 1, START_FS_DATA + loc);
    dentry_t *ptr_dentry = (dentry_t *)tmp_addr;
    // printk("now is %d\n", currentfile);
    if (1 == option)
    {
        for (int i = 0; i < MAX_DIR_OF_INODE; i++)
        {
            if (ptr_dentry->inode_id[i] != -1)
            {
                if (is_file(ptr_dentry->inode_id[i]))
                {
                    uint64_t tmp_addr0 = alloc_addr();
                    uint64_t ptr_addr0 = rw_inode(ptr_dentry->inode_id[i], tmp_addr0);
                    inode_t *ptr_inode0 = (inode_t *)ptr_addr0;
                    printk("%s ", ptr_dentry->name[i]);
                    printk("inode : %d ", ptr_dentry->inode_id[i]);
                    printk("size : %dB ", ptr_inode0->size);
                    printk("link : %d\n", ptr_inode0->link);
                    free_addr(tmp_addr0);
                }
            }
            else
            {
                if (i == 0)
                    printk("NULL\n");
                else
                    printk("\n");
                break;
            }
        }
    }
    else
    {
        for (int i = 0; i < MAX_DIR_OF_INODE; i++)
        {
            if (ptr_dentry->inode_id[i] != -1)
            {
                printk("%s ", ptr_dentry->name[i]); //,ptr_dentry->inode_id[i]);
            }
            else
            {
                if (i == 0)
                    printk("NULL\n");
                else
                    printk("\n");
                break;
            }
        }
    }

    free_addr(tmp_addr);
    return 0; // do_ls succeeds
}

int do_touch(char *path)
{
    // TODO [P6-task2]: Implement do_touch
    if (-1 == search_dir(path))
    {
        int newfile = alloc_inode_helper(1, O_RDWR, 4096);
        write_dir(currentfile, newfile, path);
    }
    else
    {
        return -1;
    }
    return 0; // do_touch succeeds
}

int do_cat(char *path)
{
    // TODO [P6-task2]: Implement do_cat
    int inode_index = search_dir(path);
    if (-1 != inode_index)
    {
        uint64_t tmp_addr = alloc_addr();
        read_file(tmp_addr, 4096, inode_index, 0);
        printk("%s", (char *)tmp_addr);
        free_addr(tmp_addr);
    }
    else
    {
        return -1;
    }
    return 0; // do_cat succeeds
}

int do_fopen(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_fopen
    for (int i = 0; i < NUM_FDESCS; i++)
    {
        if (0 == fdesc_array[i].valid)
        {
            fdesc_array[i].inode_index = search_dir(path);
            fdesc_array[i].mode = mode;
            fdesc_array[i].ptr_r = 0;
            fdesc_array[i].ptr_w = 0;
            fdesc_array[i].valid = 1;
            return i;
        }
    }
    assert(0);
    return 0; // return the id of file descriptor
}

int do_fread(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fread
    fdesc_array[fd].ptr_r += length;
    return read_file(buff, length, fdesc_array[fd].inode_index, fdesc_array[fd].ptr_r - length);
    // return the length of trully read data
}

int do_fwrite(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fwrite
    fdesc_array[fd].ptr_w += length;
    return write_file(buff, length, fdesc_array[fd].inode_index, fdesc_array[fd].ptr_w - length);
    // return the length of trully written data
}

int do_fclose(int fd)
{
    // TODO [P6-task2]: Implement do_fclose
    fdesc_array[fd].valid = 0;
    return 0; // do_fclose succeeds
}

int do_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement do_ln
    int inode_index = search_dir(src_path);
    if (-1 != inode_index)
    {
        uint64_t tmp_addr = alloc_addr();
        uint64_t ptr_addr = rw_inode(inode_index, tmp_addr);
        inode_t *ptr_inode = (inode_t *)ptr_addr;
        ptr_inode->link++;
        free_addr(tmp_addr);
        write_dir(currentfile, inode_index, dst_path);
        return 1; // do_ln succeed
    }
    return -1; // do_ln failed
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm
    return remove_file(path); // do_rm succeeds
}

int do_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement do_lseek
    if (whence == 0)
    {
        fdesc_array[fd].ptr_w = offset;
        fdesc_array[fd].ptr_r = offset;
    }
    else if (whence == 1)
    {
        fdesc_array[fd].ptr_w += offset;
        fdesc_array[fd].ptr_r += offset;
    }
    else if (whence == 2)
    {
        uint64_t tmp_addr = alloc_addr();
        inode_t *ptr_inode = (inode_t *)(rw_inode(fdesc_array[fd].inode_index, tmp_addr));
        int size = ptr_inode->size;
        fdesc_array[fd].ptr_w = size + offset;
        fdesc_array[fd].ptr_r = size + offset;
        free_addr(tmp_addr);
    }
    return 0; // the resulting offset location from the beginning of the file
}
int alloc_inode_helper(int imode, int mode, int size)
{
    // 首先要寻找一个可用inode，利用map即可
    int inode_index = alloc_inode();

    // 申请一块空间，作为存储tmp_inode的地址，然后为其内部赋值
    uint64_t tmp_addr = alloc_addr();
    int inode_block = get_inode_block(inode_index);
    inode_t *ptr_inode = (inode_t *)(rw_inode(inode_index, tmp_addr));
    ptr_inode->imode = imode;
    ptr_inode->mode = mode;
    ptr_inode->size = imode == 1 ? 0 : size;
    for (int i = 0; i < 10; i++)
    {
        ptr_inode->dir_blocks[i] = alloc_data();
    }
    ptr_inode->indir_blocks1[0] = alloc_data();
    ptr_inode->indir_blocks1[1] = alloc_data();
    ptr_inode->indir_blocks1[2] = alloc_data();
    ptr_inode->link = 1;
    bios_sdwrite(kva2pa(tmp_addr), 2, START_FS_INODE + inode_block);
    free_addr((uint64_t)tmp_addr);
    return inode_index;
}
int alloc_inode()
{
    int loc;
    uint64_t tmp_addr = alloc_addr();
    bios_sdread(kva2pa(tmp_addr), 1, START_FS_INODE_MAP);
    loc = alloc_map(tmp_addr, -1);
    if (loc == -1)
        assert(0);
    // memset(tmp_addr, 0, 512);
    // bios_sdwrite(kva2pa(tmp_addr), 1, START_FS_INODE + loc);
    free_addr(tmp_addr);
    return loc;
}
int alloc_data()
{
    int loc = -1;
    uint64_t tmp_addr = alloc_addr();
    for (int i = 0; i < NUM_FS_BLOCK_MAP; i++)
    {
        bios_sdread(kva2pa(tmp_addr), 1, START_FS_BLOCK_MAP + i);
        int idx = alloc_map(tmp_addr, i);
        if (idx == -1)
        {
            continue;
        }
        loc = i * 512 * 8 + idx;
        break;
    }
    memset(tmp_addr, 0, 4096);
    bios_sdwrite(kva2pa(tmp_addr), 8, START_FS_DATA + loc);
    free_addr(tmp_addr);
    return loc;
}

int alloc_map(uint64_t addr, int type) // type 为-1是inode，其余均为block
{
    char *ptr_addr = (char *)addr;
    int loc = -1;
    if (0xff != ptr_addr[511])
    {
        for (int j = 0; j < 512; j++)
        {
            if (0xff == ptr_addr[j])
                continue;
            else
            {
                switch (ptr_addr[j])
                {
                case 0x00:
                    ptr_addr[j] = 0x01;
                    loc = j * 8 + 0;
                    break;
                case 0x01:
                    ptr_addr[j] = 0x03;
                    loc = j * 8 + 1;
                    break;
                case 0x03:
                    ptr_addr[j] = 0x07;
                    loc = j * 8 + 2;
                    break;
                case 0x07:
                    ptr_addr[j] = 0x0f;
                    loc = j * 8 + 3;
                    break;
                case 0x0f:
                    ptr_addr[j] = 0x1f;
                    loc = j * 8 + 4;
                    break;
                case 0x1f:
                    ptr_addr[j] = 0x3f;
                    loc = j * 8 + 5;
                    break;
                case 0x3f:
                    ptr_addr[j] = 0x7f;
                    loc = j * 8 + 6;
                    break;
                case 0x7f:
                    ptr_addr[j] = 0xff;
                    loc = j * 8 + 7;
                    break;
                default:
                    assert(loc != -1);
                    break;
                }
                if (type < 0)
                {
                    bios_sdwrite(kva2pa(addr), 1, START_FS_INODE_MAP);
                }
                else
                {
                    bios_sdwrite(kva2pa(addr), 1, START_FS_BLOCK_MAP + type);
                }
                return loc;
            }
        }
    }
    return -1;
}

void write_dir(int inode_index, int child_dir, char *child_name)
{
    uint64_t tmp_addr = alloc_addr();
    uint64_t ptr_addr = rw_inode(inode_index, tmp_addr);
    inode_t *ptr_inode = (inode_t *)ptr_addr;
    int loc = ptr_inode->dir_blocks[0];
    free_addr(tmp_addr);
    tmp_addr = alloc_addr();
    bios_sdread(kva2pa(tmp_addr), 1, START_FS_DATA + loc);
    dentry_t *ptr_dentry = (dentry_t *)tmp_addr;
    for (int i = 0; i < MAX_DIR_OF_INODE; i++)
    {
        if (ptr_dentry->inode_id[i] >= 0 && 0 == strcmp(child_name, ptr_dentry->name[i]))
        {
            free_addr(tmp_addr);
            return;
        }
        if (-1 == ptr_dentry->inode_id[i])
        {
            ptr_dentry->inode_id[i] = child_dir;
            memcpy(ptr_dentry->name[i], child_name, 16);
            bios_sdwrite(kva2pa(tmp_addr), 1, START_FS_DATA + loc);
            free_addr(tmp_addr);
            return;
        }
    }
    assert(0);
}

static void init_addr()
{
    global_addr = allocFixedPage(1, current_running->pgdir);
    for (int i = 1; i < MAX_ADDR_OF_ALLOC; i++)
    {
        allocFixedPage(1, current_running->pgdir);
    }
    for (int i = 0; i < MAX_ADDR_OF_ALLOC; i++)
    {
        addr_valid[i] = 0;
    }
}

static uint64_t alloc_addr()
{
    for (int i = 0; i < MAX_ADDR_OF_ALLOC; i++)
    {
        if (0 == addr_valid[i])
        {
            addr_valid[i] = 1;
            memset(global_addr + i * 0x1000, 0, 4096);
            return global_addr + i * 0x1000;
        }
    }
    assert(0);
}
static void free_addr(uint64_t addr)
{
    int i = (addr - global_addr) / 0x1000;
    addr_valid[i] = 0;
    return;
}
static void init_dir(int inode_index)
{
    uint64_t tmp_addr = alloc_addr();
    uint64_t ptr_addr = rw_inode(inode_index, tmp_addr);
    inode_t *ptr_inode = (inode_t *)ptr_addr;

    int loc = ptr_inode->dir_blocks[0];
    free_addr(tmp_addr);
    tmp_addr = alloc_addr();
    dentry_t *ptr_dentry = (dentry_t *)tmp_addr;
    for (int i = 0; i < MAX_DIR_OF_INODE; i++)
    {
        ptr_dentry->inode_id[i] = -1;
    }
    bios_sdwrite(kva2pa(tmp_addr), 1, START_FS_DATA + loc);
    free_addr(tmp_addr);
    return;
}
static int search_dir(char *path)
{
    uint64_t tmp_addr = alloc_addr();
    uint64_t ptr_addr = rw_inode(currentfile, tmp_addr);
    inode_t *ptr_inode = (inode_t *)ptr_addr;
    int loc = ptr_inode->dir_blocks[0];
    free_addr(tmp_addr);
    tmp_addr = alloc_addr();
    dentry_t *ptr_dentry = (dentry_t *)tmp_addr;
    bios_sdread(kva2pa(tmp_addr), 1, START_FS_DATA + loc);
    for (int i = 0; i < MAX_DIR_OF_INODE; i++)
    {
        if (0 == strcmp((char *)(ptr_dentry->name[i]), path))
        {
            free_addr(tmp_addr);
            return ptr_dentry->inode_id[i];
        }
    }
    free_addr(tmp_addr);
    return -1;
}
static int enter_dir(char *path)
{
    uint64_t tmp_addr = alloc_addr();
    uint64_t ptr_addr = rw_inode(currentfile, tmp_addr);
    inode_t *ptr_inode = (inode_t *)ptr_addr;
    int loc = ptr_inode->dir_blocks[0];
    free_addr(tmp_addr);
    tmp_addr = alloc_addr();
    bios_sdread(kva2pa(tmp_addr), 1, START_FS_DATA + loc);
    dentry_t *ptr_dentry = (dentry_t *)tmp_addr;
    for (int i = 0; i < MAX_DIR_OF_INODE; i++)
    {
        if (0 == strcmp((char *)(ptr_dentry->name[i]), path))
        {
            currentfile = ptr_dentry->inode_id[i];
            free_addr(tmp_addr);
            return 1;
        }
    }
    free_addr(tmp_addr);
    return -1;
}
static int remove_dir(char *path)
{
    uint64_t tmp_addr = alloc_addr();
    uint64_t ptr_addr = rw_inode(currentfile, tmp_addr);
    inode_t *ptr_inode = (inode_t *)ptr_addr;
    int loc = ptr_inode->dir_blocks[0];
    free_addr(tmp_addr);
    tmp_addr = alloc_addr();
    bios_sdread(kva2pa(tmp_addr), 1, START_FS_DATA + loc);
    dentry_t *ptr_dentry = (dentry_t *)tmp_addr;
    for (int i = 0; i < MAX_DIR_OF_INODE; i++)
    {
        if (0 == strcmp((char *)(ptr_dentry->name[i]), path))
        {
            ptr_dentry->inode_id[i] = -1;
            bios_sdwrite(kva2pa(tmp_addr), 1, START_FS_DATA + loc);
            free_addr(tmp_addr);
            return 0;
        }
    }
    free_addr(tmp_addr);
    return -1;
}

static int read_file(char *buff, int size, int inode_index, int offset)
{
    uint64_t tmp_addr = alloc_addr();
    uint64_t ptr_addr = rw_inode(inode_index, tmp_addr);
    inode_t *ptr_inode = (inode_t *)ptr_addr;
    int start_block = loc_datablock(offset);
    int end_block = loc_datablock(offset + size - 1);
    for (int i = start_block; i <= end_block; i++)
    {
        if (i < 13)
        {
            int loc = ptr_inode->dir_blocks[i];
            uint64_t tmp_addr0 = alloc_addr();
            bios_sdread(kva2pa(tmp_addr0), 8, START_FS_DATA + loc);
            if (i == start_block)
            {
                if (i == end_block)
                {
                    memcpy(buff, tmp_addr0 + offset % PAGE_SIZE, size);
                }
                else
                {
                    memcpy(buff, tmp_addr0 + offset % PAGE_SIZE, PAGE_SIZE - offset);
                }
            }
            else if (i != end_block)
                memcpy(buff + (PAGE_SIZE - offset % PAGE_SIZE) + (i - start_block - 1) * PAGE_SIZE, tmp_addr0, PAGE_SIZE);
            else
                memcpy(buff + (PAGE_SIZE - offset % PAGE_SIZE) + (i - start_block - 1) * PAGE_SIZE, tmp_addr0, size - (PAGE_SIZE - offset % PAGE_SIZE) - (end_block - start_block - 1) * PAGE_SIZE);
            free_addr(tmp_addr0);
        }
        else
        {
            int in_dataindex = (i - 13) / 1024;
            int loc = ptr_inode->indir_blocks1[in_dataindex];
            uint64_t tmp_addr0 = alloc_addr();
            bios_sdread(kva2pa(tmp_addr0), 8, START_FS_DATA + loc);
            uint32_t *ptr_addr0 = tmp_addr0;
            int in_datablock = (i - 13) % 1024;
            if (*(ptr_addr0 + in_datablock) == 0)
            {
                int data_index = alloc_data();
                *(ptr_addr0 + in_datablock) = data_index;
                bios_sdwrite(kva2pa(tmp_addr0), 8, START_FS_DATA + loc);
            }
            int data_index = *(ptr_addr0 + in_datablock);
            free_addr(tmp_addr0);
            tmp_addr0 = alloc_addr();
            bios_sdread(kva2pa(tmp_addr0), 8, START_FS_DATA + data_index);
            if (i == start_block)
            {
                if (i == end_block)
                {
                    memcpy(buff, tmp_addr0 + offset % PAGE_SIZE, size);
                }
                else
                {
                    memcpy(buff, tmp_addr0 + offset % PAGE_SIZE, PAGE_SIZE - offset % PAGE_SIZE);
                }
            }

            else if (i != end_block)
                memcpy(buff + (PAGE_SIZE - offset % PAGE_SIZE) + (i - start_block - 1) * PAGE_SIZE, tmp_addr0, PAGE_SIZE);
            else
                memcpy(buff + (PAGE_SIZE - offset % PAGE_SIZE) + (i - start_block - 1) * PAGE_SIZE, tmp_addr0, size - (PAGE_SIZE - offset % PAGE_SIZE) - (end_block - start_block - 1) * PAGE_SIZE);
            free_addr(tmp_addr0);
        }
    }
    free_addr(tmp_addr);
    return size;
}

static int write_file(char *buff, int size, int inode_index, int offset)
{
    uint64_t tmp_addr = alloc_addr();
    int inode_block = get_inode_block(inode_index);
    uint64_t ptr_addr = rw_inode(inode_index, tmp_addr);
    inode_t *ptr_inode = (inode_t *)ptr_addr;
    int start_block = loc_datablock(offset);
    int end_block = loc_datablock(offset + size - 1);
    if (offset + size > ptr_inode->size)
    {
        ptr_inode->size = offset + size;
    }
    for (int i = start_block; i <= end_block; i++)
    {
        if (i < 13)
        {
            int loc = ptr_inode->dir_blocks[i];
            uint64_t tmp_addr0 = alloc_addr();
            bios_sdread(kva2pa(tmp_addr0), 8, START_FS_DATA + loc);
            if (i == start_block)
            {
                if (i == end_block)
                {
                    memcpy(tmp_addr0 + offset % PAGE_SIZE, buff, size);
                }
                else
                {
                    memcpy(tmp_addr0 + offset % PAGE_SIZE, buff, PAGE_SIZE - offset);
                }
            }
            else if (i != end_block)
                memcpy(tmp_addr0, buff + (PAGE_SIZE - offset % PAGE_SIZE) + (i - start_block - 1) * PAGE_SIZE, PAGE_SIZE);
            else
                memcpy(tmp_addr0, buff + (PAGE_SIZE - offset % PAGE_SIZE) + (i - start_block - 1) * PAGE_SIZE, size - (PAGE_SIZE - offset % PAGE_SIZE) - (end_block - start_block - 1) * PAGE_SIZE);
            bios_sdwrite(kva2pa(tmp_addr0), 8, START_FS_DATA + loc);
            free_addr(tmp_addr0);
        }
        else
        {
            int in_dataindex = (i - 13) / 1024;
            int loc = ptr_inode->indir_blocks1[in_dataindex];
            uint64_t tmp_addr0 = alloc_addr();
            bios_sdread(kva2pa(tmp_addr0), 8, START_FS_DATA + loc);
            uint32_t *ptr_addr0 = tmp_addr0;
            int in_datablock = (i - 13) % 1024;
            if (*(ptr_addr0 + in_datablock) == 0)
            {
                int data_index = alloc_data();
                *(ptr_addr0 + in_datablock) = data_index;
            }
            int data_index = *(ptr_addr0 + in_datablock);
            bios_sdwrite(kva2pa(tmp_addr0), 8, START_FS_DATA + loc);
            free_addr(tmp_addr0);
            tmp_addr0 = alloc_addr();
            bios_sdread(kva2pa(tmp_addr0), 8, START_FS_DATA + data_index);
            if (i == start_block)
            {
                if (i == end_block)
                {
                    memcpy(tmp_addr0 + offset % PAGE_SIZE, buff, size);
                }
                else
                {
                    memcpy(tmp_addr0 + offset % PAGE_SIZE, buff, PAGE_SIZE - offset % PAGE_SIZE);
                }
            }
            else if (i != end_block)
                memcpy(tmp_addr0, buff + (PAGE_SIZE - offset) + (i - start_block - 1) * PAGE_SIZE, PAGE_SIZE);
            else
                memcpy(tmp_addr0, buff + (PAGE_SIZE - offset % PAGE_SIZE) + (i - start_block - 1) * PAGE_SIZE, size - (PAGE_SIZE - offset % PAGE_SIZE) - (end_block - start_block - 1) * PAGE_SIZE);
            bios_sdwrite(kva2pa(tmp_addr0), 8, START_FS_DATA + data_index);
            free_addr(tmp_addr0);
        }
    }
    bios_sdwrite(kva2pa(tmp_addr), 2, START_FS_INODE + inode_block);
    free_addr(tmp_addr);
    return size;
}
// static int copy_inode(int inode_index)
// {
//     // 首先要寻找一个可用inode，利用map即可
//     int inode_index = alloc_inode();

//     // 申请一块空间，作为存储tmp_inode的地址，然后为其内部赋值
//     inode_t *ptr_inode = (inode_t *)(alloc_addr());
//     uint64_t tmp_addr = alloc_addr();
//     bios_sdread(kva2pa(tmp_addr), 1, START_FS_INODE + inode_index);
//     inode_t *ptr_inode_ori = (inode_t *)(tmp_addr);
//     ptr_inode->imode = ptr_inode_ori->imode;
//     ptr_inode->mode = ptr_inode_ori->mode;
//     ptr_inode->size = ptr_inode_ori->size;
//     for (int i = 0; i < 10; i++)
//     {
//         ptr_inode->dir_blocks[i] = ptr_inode_ori->dir_blocks[i];
//     }
//     ptr_inode->indir_blocks[0] = ptr_inode_ori->dir_blocks[0];
//     ptr_inode->indir_blocks[1] = ptr_inode_ori->dir_blocks[1];
//     ptr_inode->indir_blocks[2] = ptr_inode_ori->dir_blocks[2];
//     bios_sdwrite(kva2pa((uint64_t)ptr_inode), 1, START_FS_INODE + inode_index);
//     free_addr((uint64_t)ptr_inode);
//     free_addr((uint64_t)ptr_inode_ori);
//     return inode_index;
// }

static int get_inode_offset(int ino)
{
    int ino_num = ino * sizeof(struct inode_t);
    return ino_num % 512;
}

static int get_inode_block(int ino)
{
    int ino_num = ino * sizeof(struct inode_t);
    return ino_num / 512;
}

static uint64_t rw_inode(int ino, uint64_t addr)
{
    int block_id = get_inode_block(ino);
    int offset = get_inode_offset(ino);
    bios_sdread(kva2pa(addr), 2, START_FS_INODE + block_id);
    return (addr + offset);
}

static int is_file(int ino)
{
    uint64_t tmp_addr = alloc_addr();
    uint64_t ptr_addr = rw_inode(ino, tmp_addr);
    inode_t *ptr_inode = (inode_t *)ptr_addr;
    int is_file = (ptr_inode->imode == 1);
    free_addr(tmp_addr);
    return is_file;
}

static int remove_file(char *path)
{
    uint64_t tmp_addr = alloc_addr();
    uint64_t ptr_addr = rw_inode(currentfile, tmp_addr);
    inode_t *ptr_inode = (inode_t *)ptr_addr;
    int loc = ptr_inode->dir_blocks[0];
    free_addr(tmp_addr);
    tmp_addr = alloc_addr();
    bios_sdread(kva2pa(tmp_addr), 1, START_FS_DATA + loc);
    dentry_t *ptr_dentry = (dentry_t *)tmp_addr;
    for (int i = 0; i < MAX_DIR_OF_INODE; i++)
    {
        if (0 == strcmp((char *)(ptr_dentry->name[i]), path))
        {
            if (is_file(ptr_dentry->inode_id[i]))
            {
                int ino = ptr_dentry->inode_id[i];
                ptr_dentry->inode_id[i] = -1;
                bios_sdwrite(kva2pa(tmp_addr), 1, START_FS_DATA + loc);
                free_addr(tmp_addr);
                tmp_addr = alloc_addr();
                uint64_t ptr_addr = rw_inode(ino, tmp_addr);
                inode_t *ptr_inode = (inode_t *)ptr_addr;
                ptr_inode->link--;
                if (ptr_inode->link <= 0)
                {
                }
                free_addr(tmp_addr);
                return 0;
            }
        }
    }
    free_addr(tmp_addr);
    return -1;
}
static int free_inode(int ino)
{
}
static int free_data(int data_index)
{
}
static int loc_datablock(int size)
{
    // if (size < 4096 * 13)
    // {
    //     return size / 4096;
    // }
    // else if (size < 4096 * 13 + 4096 * 1024 * 3)
    // {
    //     return (size - 4096 * 13) / (4096 * 1024) + 13;
    // }
    // else if (size < 4096 * 13 + 4096 * 1024 * 3 + 4096 * 1024 * 1024 * 2)
    // {
    //     return (size - 4096 * 13 - 4069 * 1024 * 3) / (4096 * 1024 * 1024) + 16;
    // }
    // else
    //     return 18;
    return size / 4096;
}

void do_clearSD()
{
    uint64_t tmp_addr = alloc_addr();
    for (int i = START_FS; i < START_FS + 0x100000; i += 64)
    {
        bios_sdwrite(tmp_addr, 64, i);
    }
    free_addr(tmp_addr);
}