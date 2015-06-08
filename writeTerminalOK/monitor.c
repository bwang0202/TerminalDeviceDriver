#include <stdio.h>
#include <stdlib.h>
#include <hardware.h>
#include <threads.h>

#define SIZE 200
#define ECHOSIZE 200
//each input_buffer: only writable by main terminal thread, but can be
//read by multiple user thread.
static char *input_buffer[MAX_NUM_TERMINALS - 1];
static int inputbuffer_size[MAX_NUM_TERMINALS - 1];
static int inputbuffer_ridx[MAX_NUM_TERMINALS - 1];
static int inputbuffer_widx[MAX_NUM_TERMINALS - 1];
static char *echo_buffer[MAX_NUM_TERMINALS - 1];
static int echobuffer_size[MAX_NUM_TERMINALS - 1];
static int echobuffer_ridx[MAX_NUM_TERMINALS - 1];
static int echobuffer_widx[MAX_NUM_TERMINALS - 1];
static int busy[MAX_NUM_TERMINALS - 1];
static cond_id_t busycond[MAX_NUM_TERMINALS - 1];

int write_to_buffer1(char *buffer, char c, int i, int *buffer_size, int* widx, int max) {
        if(buffer_size[i] == max) return 0;
        buffer[widx[i]] = c;
        widx[i] = (widx[i] + 1) % max;
        buffer_size[i]++;
        return 1;
}

char read_buffer1(char *buffer, int i, int *buffer_size, int* ridx, int max) { 
        char c;
        if(buffer_size[i] == 0) return 0;
        c = buffer[ridx[i]];
        ridx[i] = (ridx[i] + 1) % max;
        buffer_size[i]--;
        return c;
}

extern int
write_char_inputbuffer(int term, char c){
//        Declare_Monitor_Entry_Procedure();
        return write_to_buffer1(input_buffer[term], c, term, inputbuffer_size, inputbuffer_widx, SIZE);
}

extern char
read_inputbuffer(int term){
      Declare_Monitor_Entry_Procedure();
      return read_buffer1(input_buffer[term], term, inputbuffer_size, inputbuffer_ridx, SIZE);
}


extern void
init_moniterminal(int term){
	inputbuffer_size[term] = 0;
	inputbuffer_ridx[term] = 0;
	inputbuffer_widx[term] = 0;
	input_buffer[term] = malloc(sizeof(char) * SIZE);
	echo_buffer[term] = malloc(sizeof(char) * ECHOSIZE);
        echobuffer_size[term] = 0;
        echobuffer_ridx[term] = 0;
        echobuffer_widx[term] = 0;
	busy[term] = 0;
	busycond[term] = CondCreate();
}

extern void
ti_inner(term){
	Declare_Monitor_Entry_Procedure();
	busy[term] = 0;
	CondSignal(busycond[term]);
	char c = read_buffer1(echo_buffer[term], term, echobuffer_size, echobuffer_ridx, ECHOSIZE);
	if (c) {
		printf("[0]: %c\n", c);
		WriteDataRegister(term, c);
		
		busy[term] = 1;
	}
	return;
}

extern void
ri_inner(term){
	Declare_Monitor_Entry_Procedure();
	char c = ReadDataRegister(term);
	if (write_char_inputbuffer(term, c)) {
		if (busy[term]) {
		write_to_buffer1(echo_buffer[term], c, term, echobuffer_size, echobuffer_widx, ECHOSIZE);
		} else {
			WriteDataRegister(term, c);
			busy[term] = 1;
		}
	}
}
extern int
wt_inner(int term, char *buf, int buflen){
	Declare_Monitor_Entry_Procedure();
	int result = 0;
	char c;
	while (result < buflen){
		while(busy[term]){CondWait(busycond[term]);}
		c = read_buffer1(echo_buffer[term], term, echobuffer_size, echobuffer_ridx, ECHOSIZE);
	        if (c) {
        	        WriteDataRegister(term, c);                             
                	busy[term] = 1;
			CondWait(busycond[term]);                                         
        	} else {
			WriteDataRegister(term, buf[result]);
			busy[term] = 1;
			result++;
			CondWait(busycond[term]);
		}
	
	}
	return result;
}


/*
extern int
write_char_inputbuffer(int term, char c){
	//Declare_Monitor_Entry_Procedure();
	return write_to_buffer1(input_buffer[term], c, term, inputbuffer_size, inputbuffer_widx, SIZE);
}

extern char
read_inputbuffer(int term){
	Declare_Monitor_Entry_Procedure();
	return read_buffer1(input_buffer[term], term, inputbuffer_size, inputbuffer_ridx, SIZE); 
}
*/                              
