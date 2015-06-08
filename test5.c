#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <terminals.h>

void writer1(void *arg);
int
main(int argc, char **argv)
{
	int len1, len2;
        char buf1[2];
        char buf2[10];
InitTerminalDriver();
InitTerminal(0);   

     len1 = ReadTerminal(0, buf1, 2);
   // status = WriteTerminal(1, string1, length1);
    len2 = ReadTerminal(0, buf2, 10);
   //     
   // if (argc > 1) HardwareOutputSpeed(1, atoi(argv[1]));
   // if (argc > 2) HardwareInputSpeed(1, atoi(argv[2]));

   // ThreadCreate(writer1, NULL);
 //  ThreadCreate(writer2, NULL);
 printf("len1 %d, %c %c,  len2 %d, %c %c %c\n", len1, buf1[0],buf1[1],len2, buf2[0],buf2[1],buf2[9]);

    ThreadWaitAll();

    exit(0);
}
/*
void
writer1(void *arg)
{
   // int status;
	int len1, len2;
	char buf1[2];
	char buf2[10];
	len1 = ReadTerminal(0, buf1, 2);
   // status = WriteTerminal(1, string1, length1);
    len2 = ReadTerminal(0, buf2, 10);
	printf("len1 %d, %c %c,  len2 %d, %c %c %c\n", len1, buf1[0],buf2[0],len2, buf2[0],buf2[1],buf2[9]);
}*/
/*
void
writer2(void *arg)
{
    int status;

    status = WriteTerminal(1, string2, length2);
    if (status != length2)
	fprintf(stderr, "Error: writer2 status = %d, length2 = %d\n",
	    status, length2);
}
*/
