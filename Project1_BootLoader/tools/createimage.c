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
    char file_name[8];
    int offset;
    int file_size;
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

        //write info into
        if(taskidx>=0)
        {
        	int len = strlen(*files);
        	memcpy(taskinfo[taskidx].file_name, *files, len);
        	taskinfo[taskidx].file_name[len] = '\0';
        	printf("record name is %s",taskinfo[taskidx].file_name);
		taskinfo[taskidx].offset = phyaddr;
	}
	
	printf("write %s taskinfo\n",*files);
	printf("offset is to %d\n",taskinfo[taskidx].offset);


        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);
            
            printf("begin");

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);
            
            //debug
            printf("end");

            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0) {
                nbytes_kernel += get_filesz(phdr);
            }

        }
        

        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */
        /*  task3_code: padding to 1 or 15 sector
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }
        else{
            write_padding(img, &phyaddr, 15 * SECTOR_SIZE * fidx + SECTOR_SIZE);
        }
        */

        //task4 --> only padding bootblock
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }

	if(taskidx>=0)
		taskinfo[taskidx].file_size = phyaddr - taskinfo[taskidx].offset;

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
        printf("\t\twrite 0x%04lx bytes for padding\n", new_phyaddr - *phyaddr);
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
    short num_of_sectors = (nbytes_kern / SECTOR_SIZE) + ((nbytes_kern % SECTOR_SIZE) != 0);
    printf("\nnum_of_sectors is %d\n",num_of_sectors);
    //debug
    fseek(img,OS_SIZE_LOC,SEEK_SET);
    fwrite(&num_of_sectors,2,1,img);
    printf("write num_of_sectors_of_kernel\n");

    for(int i=0; i<tasknum; i++){
        printf("app %d: %s\n",i,taskinfo[i].file_name);
        printf("offset is to: %d\n",taskinfo[i].offset);
        printf("file_size is %d\n",taskinfo[i].file_size);
    }

    printf("\nsizeof(task_info_t) is %ld\n", sizeof(task_info_t));

    //task4_code
    int task_info_loc = taskinfo[tasknum-1].offset + taskinfo[tasknum-1].file_size;  //info placed after the last app
    short start_block = task_info_loc / SECTOR_SIZE;
    short size_of_info = sizeof(task_info_t) * tasknum;
    int end_loc = task_info_loc + size_of_info;
    short num_of_sector_of_info = (end_loc / SECTOR_SIZE + ( (end_loc % SECTOR_SIZE) != 0 )) - start_block;
    short offset_of_info = task_info_loc - start_block * SECTOR_SIZE;

    /*store
     * short start_block
     * short num_of_sector_of_info
     * short offset_of_info
    */ 

    printf("\nloc is %d\n",task_info_loc);
    printf("\nos_size_loc is: %d\n",OS_SIZE_LOC-4);
    fseek(img, OS_SIZE_LOC-6, SEEK_SET);
    fwrite(&start_block, 2, 1, img);
    fwrite(&num_of_sector_of_info, 2, 1, img);
    fwrite(&offset_of_info, 2, 1, img);

    fseek(img, task_info_loc, SEEK_SET);
    fwrite(taskinfo, sizeof(task_info_t) * tasknum, 1, img);
    printf("write task_info\n");

    FILE *fp = NULL;
    fp = fopen("/home/stu/chenyuanteng20/Project1_BootLoader/batch1.txt","r");
    if(fp==NULL)
	    printf("fopen error");
    fseek(img, OS_SIZE_LOC-11, SEEK_SET);
    int count = 5;
    char c;
    while(count > 0){
	    c = fgetc(fp);
	    printf("c is %c\n",c);
	    fwrite(&c, sizeof(char), 1, img);
	    count--;
    }
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
