#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa


#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    /*
    char file_name[20];
    int offset;
    int file_size;
    */

    //in prj4, we need more info of task
    int offset;
    int file_size;   //在ELF中的大小
    int mem_size;    //在虚拟卡空间中的大小(>=filesz)
    int vaddr;    //第一个字节在进程虚拟空间的起始地址
    char file_name[20];
} task_info_t;


#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img);

int main(int argc, char **argv)
{
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}

/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 2;
    int nbytes_kernel = 0;
    int phyaddr = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /* for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 2;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        //write file_name and offset into struct taskinfo
        if(taskidx>=0)
        {
        	int len = strlen(*files);
        	memcpy(taskinfo[taskidx].file_name, *files, len);
        	taskinfo[taskidx].file_name[len] = '\0';
        	printf("record name is %s",taskinfo[taskidx].file_name);
		    taskinfo[taskidx].offset = phyaddr;

            //初始化file_size和mem_size
            taskinfo[taskidx].file_size = 0;
            taskinfo[taskidx].mem_size = 0;
	    }
	
	    // print some info for debug
	    printf("write %s taskinfo\n",*files);
	    printf("offset is to %d\n",taskinfo[taskidx].offset);


        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            /*在加载可执行文件时，我们只需要关心p_type为LOAD类型
            的字段，在我们的实验框架中，为了简单，只存在一个LOAD类型的程序段
            */

            if(phdr.p_flags % 2 != 1)
            {
                //not LOAD_TYPE
                continue;
            }

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);

            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0) {
                nbytes_kernel += get_filesz(phdr);
            }
            else if(taskidx >= 0)
            {
                taskinfo[taskidx].file_size += get_filesz(phdr);
                //因为就一个LOAD段，所以vaddr实际只被赋值一次
                taskinfo[taskidx].vaddr = phdr.p_vaddr;
                taskinfo[taskidx].mem_size += phdr.p_memsz;
            }
        }

        //task4 --> only padding bootblock
        if (strcmp(*files, "bootblock") == 0) {
	    //in task4-5,only bootblock need padding to 1 block
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }

        
        if (strcmp(*files, "main") == 0) {
            int padding_end;
            padding_end = (phyaddr / SECTOR_SIZE + 1) * SECTOR_SIZE;
            write_padding(img, &phyaddr, padding_end);
        }
              

        /*
	    if(taskidx>=0){
		    //file_size = 前后phyaddr之差
		    taskinfo[taskidx].file_size = phyaddr - taskinfo[taskidx].offset;
	    }
        */

        fclose(fp);
        files++;
    }
    write_img_info(nbytes_kernel, taskinfo, tasknum, img);

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_img_info(int nbytes_kern, task_info_t *taskinfo,
                           short tasknum, FILE * img)
{
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...
    printf("\nnbytes_of_kernel is %d",nbytes_kern);
    //计算内核所占扇区数
    short num_of_sectors = (nbytes_kern / SECTOR_SIZE) + ((nbytes_kern % SECTOR_SIZE) != 0);
    printf("\nnum_of_sectors of kernel is %d\n",num_of_sectors);
    
    //指针指向bootblock中写num_of_sector的位置
    fseek(img,OS_SIZE_LOC,SEEK_SET);
    //write num_of_sectors
    fwrite(&num_of_sectors,2,1,img);
    printf("write num_of_sectors_of_kernel\n");

    //print img info so that we can check easily
    for(int i=0; i<tasknum; i++){
        printf("app %d: %s\n",i,taskinfo[i].file_name);
        printf("offset is to: %d\n",taskinfo[i].offset);
        printf("file_size is %d\n",taskinfo[i].file_size);
        printf("mem_size is %d\n",taskinfo[i].mem_size);
        printf("vaddr is %d\n",taskinfo[i].vaddr);
    }

    //print size of struct
    printf("\nsizeof(task_info_t) is %ld\n", sizeof(task_info_t));

    /*以下计算四个要记录的值
     *1.start_block: 开始记录info的扇区
     *2.num_of_sector_of_info: 记录info所占的扇区数
     *3.offset_of_info: info真正开始的位置距离start_block的偏移量
     *4.tasknum 不用计算，但是需要记录，因为增加app时会变
    */

    //首先是开始记录info的地址（image中）
    int task_info_loc = taskinfo[tasknum-1].offset + taskinfo[tasknum-1].file_size;  //info placed after the last app
    //开始记录info的扇区
    short start_block = task_info_loc / SECTOR_SIZE;
    //info的大小
    short size_of_info = sizeof(task_info_t) * tasknum;
    //记录info结束的扇区号
    int end_loc = task_info_loc + size_of_info;
    //所用扇区数
    short num_of_sector_of_info = (end_loc / SECTOR_SIZE + ( (end_loc % SECTOR_SIZE) != 0 )) - start_block;
    //info开始的地址距离start_block的偏移量
    short offset_of_info = task_info_loc - start_block * SECTOR_SIZE;

    /*store
     * short tasknum
     * short start_block
     * short num_of_sector_of_info
     * short offset_of_info
    */ 

    //print some info for debug
    printf("\nloc is %d\n",task_info_loc);
    printf("\nos_size_loc is: %d\n",OS_SIZE_LOC-4);

    //store 4 short int for 8 bytes in total
    //so start addr is OS_SIZE_LOC - 8
    fseek(img, OS_SIZE_LOC-8, SEEK_SET);

    //write 4 short int in order

    //place info 1
    //write tasknum for adding test app
    printf("task num is %d\n",tasknum);
    fwrite(&tasknum, 2, 1, img);

    //place info 2
    //write start_block
    fwrite(&start_block, 2, 1, img);

    //place info 3
    //write num_of_sector_of_info
    fwrite(&num_of_sector_of_info, 2, 1, img);

    //place info 4
    //write offset_of_info
    fwrite(&offset_of_info, 2, 1, img);


    //image info
    //write image info at task_info_loc
    fseek(img, task_info_loc, SEEK_SET);
    fwrite(taskinfo, sizeof(task_info_t) * tasknum, 1, img);
    printf("write task_info finish!\n");


    //for task5--batch_processing

    /*
    //read info of batch processing
    FILE *fp = NULL;
    fp = fopen("batch1.txt","r");

    if(fp==NULL)
	    printf("fopen error");
    //write batch processing info in end of bootblock
    fseek(img, OS_SIZE_LOC-20, SEEK_SET);
    char c;
    while(c!='x'){
	    c = fgetc(fp);
	    if((c!='x') && (c-'0')>tasknum)
            {
		    printf("taskid error! please edit batch1.txt again!");
		    exit(1);
	    }
	    printf("taskid is %c\n",c);
	    fwrite(&c, sizeof(char), 1, img);
    }
    */
}

/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}
