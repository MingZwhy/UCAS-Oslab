#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <type.h>

/* macros of file system */
#define SUPERBLOCK_MAGIC 0x20221206
#define NUM_FDESCS 16
#define NUM_DENTRY 64
#define START_FS   0x100000
#define START_FS_BLOCK_MAP 0x100000 + 1
#define START_FS_INODE_MAP 0x100000 + 33
#define START_FS_INODE 0x100000 + 34
#define START_FS_DATA 0x100000 + 34 + 2048

#define NUM_FS_BLOCK_MAP 4 * 8
#define NUM_FS_INODE_MAP 1

#define MAX_DIR_OF_INODE 16
#define MAX_ADDR_OF_ALLOC 8


#define PAGE_SIZE 4096
#define K 1024
/* data structures of file system */
typedef struct superblock_t{
    // TODO [P6-task1]: Implement the data structure of superblock
    int magic_number;
    int size;
    
    //block map
    int startaddr_blockmap;
    int num_of_blockmap;
    //inodes map
    int startaddr_inodemap;
    int num_of_inodemap;
    //inodes
    int startaddr_inodes;
    int num_of_inodes;
    //data
    int startaddr_data;
    int num_of_data;
} superblock_t;

typedef struct dentry_t{
    // TODO [P6-task1]: Implement the data structure of directory entry
    int inode_id[MAX_DIR_OF_INODE];
    char name[16][MAX_DIR_OF_INODE];
} dentry_t;

typedef struct inode_t{ 
    // TODO [P6-task1]: Implement the data structure of inode
    int imode;  //1是文件，0是目录
    int mode;
    int ownerinfo;
    int size;
    int timestamps;
    int dir_blocks[13];
    int indir_blocks1[3];
    int indir_blocks2[2];
    int indir_blocks3[1];
    int link;
} inode_t;

typedef struct fdesc_t{
    // TODO [P6-task2]: Implement the data structure of file descriptor
    int inode_index;
    int mode;
    int ptr_r;
    int ptr_w;
    int valid;
} fdesc_t;

/* modes of do_fopen */
#define O_RDONLY 1  /* read only open */
#define O_WRONLY 2  /* write only open */
#define O_RDWR   3  /* read/write open */

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* fs function declarations */
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_touch(char *path);
extern int do_cat(char *path);
extern int do_fopen(char *path, int mode);
extern int do_fread(int fd, char *buff, int length);
extern int do_fwrite(int fd, char *buff, int length);
extern int do_fclose(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);
void do_clearSD(void);

#endif