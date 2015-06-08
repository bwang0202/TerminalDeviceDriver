#include <stdio.h>
#include <stdlib.h>
#include <hardware.h>
#include <threads.h>

#define SIZE 200
//each input_buffer: only writable by main terminal thread, but can be
//read by multiple user thread.
static char *input_buffer[MAX_NUM_TERMINALS - 1];
static int inputbuffer_size[MAX_NUM_TERMINALS - 1];
static int inputbuffer_ridx[MAX_NUM_TERMINALS - 1];
static int inputbuffer_widx[MAX_NUM_TERMINALS - 1];

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


extern void
init_moniterminal(int term){
	inputbuffer_size[term] = 0;
	inputbuffer_ridx[term] = 0;
	inputbuffer_widx[term] = 0;
	input_buffer[term] = malloc(sizeof(char) * SIZE);
}

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


