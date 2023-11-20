#include<stdio.h>
#include<stdlib.h>
#include<string.h>

int main()
{
	FILE *fp=NULL;
	fp = fopen("./build/batch1.txt","w");
	if(fp==NULL)
	{
		printf("open batch1.txt error!");
		exit(1);
	}
	char c = ' ';
	printf("please input batch process (end with 'x')\n");
	printf("hint: taskid begin as 1 which means there is not task0\n");
	printf("if you input num > tasknum, creatimage will exit\n");
	printf("for example: 1324x\n");
	printf("hint: you can run 10 tasks at most one time, so don't input more than 10 taskid\n");
	printf("start writing into batch1.txt:\n");
	
	int count = 0;
	while(c!='x')
	{
		c = getchar();
		count++;
		if(count > 10)
		{
			printf("You have wrote taskid more than 10, stop writing into batch1.txt\n");
			c = 'x';
			fwrite(&c, sizeof(char), 1, fp);
			return 0;
		}
		fwrite(&c, sizeof(char), 1, fp);
	}
	return 0;
}
