
#include <stdio.h>


FILE *fp;

int main()
{
	unsigned char data[1024];
	fp = fopen("ok.aac", "r");
	long cur ;
	fseek(fp,0,SEEK_CUR);
	cur = ftell(fp);
	printf("%d\r\n", cur);
	rewind(fp);

	int readn = fread(data, 1, 1, fp);
	cur = ftell(fp);
	printf("%d\r\n", cur);
	return 0;
}
