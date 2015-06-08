#include <stdio.h>
#include <stdlib.h>
#include <hardware.h>
#include <terminals.h>
#include "monitor.c"

//#define ECHOSIZE 100
//only the main terminal thread could modify this flag.
//static int busy[MAX_NUM_TERMINALS - 1];
//only the main terminal thread can modify echo_buffer.
//static char *echo_buffer[MAX_NUM_TERMINALS - 1];
//static int echobuffer_size[MAX_NUM_TERMINALS - 1];
//static int echobuffer_ridx[MAX_NUM_TERMINALS - 1];
//static int echobuffer_widx[MAX_NUM_TERMINALS - 1];

int write_to_buffer2(char *buffer, char c, int i, int *buffer_size, int* widx, int max) {
        if(buffer_size[i] == max) return 0;
        buffer[widx[i]] = c;
        widx[i] = (widx[i] + 1) % max;
        buffer_size[i]++;
        return 1;
}

char read_buffer2(char *buffer, int i, int *buffer_size, int* ridx, int max) { 
        char c;
        if(buffer_size[i] == 0) return 0;
        c = buffer[ridx[i]];
        ridx[i] = (ridx[i] + 1) % max;
        buffer_size[i]--;
        return c;
}


void ReceiveInterrupt(int term){
/*	char c;
	c = ReadDataRegister(term);
	if (write_char_inputbuffer(term, c)){
		if (busy[term]) {
			write_to_buffer2(echo_buffer[term], c, term, echobuffer_size, echobuffer_widx, ECHOSIZE);
		} else {
			WriteDataRegister(term, c);
			busy[term] = 1;
		}
	}	
*/	return ri_inner(term);
}
void TransmitInterrupt(int term){
//	char c;
//	busy[term] = 0;
	//if echo buffer is not empty, pop, writedataregister, setbusy
/*	c = read_buffer2(echo_buffer[term], term, echobuffer_size, echobuffer_ridx, ECHOSIZE);
	if (c) {
		WriteDataRegister(term, c);
		busy[term] = 1;
		return;
	}
*/	//if echo empty, write output buffer and setbusy
	
	//if both empty, return.
	return ti_inner(term);
}
int WriteTerminal(int term, char *buf, int buflen){
	char *output_buffer = buf;
	int output_len = buflen;
	return wt_inner(term, output_buffer, output_len);
}
int ReadTerminal(int term, char *buf, int buflen){return 0;}
int InitTerminal(int term){
	InitHardware(term);
	init_moniterminal(term);
//	busy[term] = 0;
//	echo_buffer[term] = malloc(sizeof(char) * ECHOSIZE);
//	echobuffer_size[term] = 0;
//	echobuffer_ridx[term] = 0;
//	echobuffer_widx[term] = 0;	
	return 0;
}
int TerminalDriverStatistics(struct termstat *stat){return 0;}
int InitTerminalDriver(){return 0;}














