#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define M_8 8388608

static char buff[64];

int main(void)
{
    int fd = sys_fopen("1.txt", O_RDWR);

    // Test 1:Write bigger file, not very big, just 8KB
    sys_move_cursor(0, 0);
    for (int i = 0; i < 512; i++)
    {
        sys_fwrite(fd, "1234567890abcde\n", 16);
    }
    sys_move_cursor(0, 0);
    printf("Pass 1...\n");
    // Test 2:Write something in 8MB, this starts in 8MB, only occupy one block
    sys_lseek(fd, M_8, 0);
    char *s0 = "ts_test_test2!!\n";
    sys_fwrite(fd, s0, 16);
    sys_lseek(fd, M_8, 0);
    sys_fread(fd, buff, 16);
    if (strcmp(s0, buff) == 0)
    {
        printf("Pass 2...\n");
    }
    else
    {
        printf("Error in 2!\n");
        return 0;
    }
    // Test 3:Write something in 8MB, this starts in 8MB-1, occupy two blocks
    sys_lseek(fd, M_8 - 1, 0);
    char *s1 = "Otest_test_ts!!\n";
    sys_fwrite(fd, s1, 16);
    sys_lseek(fd, M_8 - 1, 0);
    sys_fread(fd, buff, 16);
    if (strcmp(s1, buff) == 0)
    {
        printf("Pass 3...\n");
    }
    else
    {
        printf("Error in 3!\n");
        return 0;
    }
    // Test 4:Test lseek when whence is 1
    sys_lseek(fd, M_8 - 1, 0);
    sys_lseek(fd, 1, 1);
    char *s2 = "test_test_ts!!\n";
    sys_fread(fd, buff, 15);
    buff[15] = '\0';
    if (strcmp(s2, buff) == 0)
    {
        printf("Pass 4...\n");
    }
    else
    {
        printf("Error in 4!\n");
        return 0;
    }

    // Test 5:Test lseek when whence is 2
    sys_lseek(fd, 16, 2);
    char *s3 = "aabbccddeeffg!!\n";
    sys_fwrite(fd, s3, 16);
    sys_fread(fd, buff, 16);
    if (strcmp(s3, buff) == 0)
    {
        printf("Pass 5...\n");
    }
    else
    {
        printf("Error in 5!\n");
        return 0;
    }
    printf("Succeed!\n");
    sys_fclose(fd);

    return 0;
}