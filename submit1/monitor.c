#include <stdio.h>
#include <stdlib.h>
#include <hardware.h>
#include <threads.h>

#define SIZE 200
#define ECHOSIZE 200

struct box {
	int initialized;
	int tin;
	int tout;
	int uin;
	int uout;
};
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
static int wt_ocpd[MAX_NUM_TERMINALS - 1];
static cond_id_t wtcond[MAX_NUM_TERMINALS - 1];
static int rt_ocpd[MAX_NUM_TERMINALS - 1];
static cond_id_t rtcond[MAX_NUM_TERMINALS - 1];
static int tds_ocpd;
static cond_id_t tdscond;

static cond_id_t inputempty[MAX_NUM_TERMINALS - 1];
static struct box *prev;
static struct box *curr;
static struct box *next;

extern void
tds(struct termstat* stat){
	Declare_Monitor_Entry_Procedure();
	struct box *old;
	int i;
	while(tds_ocpd){CondWait(tdscond);}
	tds_ocpd = 1;
	old = prev;
	prev = curr;
	curr = next; //after this execution, everything will be written into next
	for (i = 0; i < MAX_NUM_TERMINALS; i++){
		//prev += old, then free old
		if (old[i].initialized){
			prev[i].initialized = 1;
			prev[i].tin +=old[i].tin;
			prev[i].tout +=old[i].tout;
			prev[i].uin += old[i].uin;
			prev[i].uout += old[i].uout;
		}
		if (prev[i].initialized){
			//copy data into stat
			stat[i].tty_in = prev[i].tin;
			stat[i].tty_out = prev[i].tout;
			stat[i].user_in = prev[i].uin;
			stat[i].user_out = prev[i].uout;
		} else {
			//copy -1 into stat
			stat[i].tty_in = -1;
			stat[i].tty_out = -1;
			stat[i].user_in = -1;
			stat[i].user_out = -1;
		}
	}
	free(old);
	next = calloc(MAX_NUM_TERMINALS, sizeof(struct box));
	tds_ocpd = 0;
	CondSignal(tdscond);
	return;	
}

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

int
write_char_inputbuffer(int term, char c){
       	CondSignal(inputempty[term]);
        return write_to_buffer1(input_buffer[term], c, term, inputbuffer_size, inputbuffer_widx, SIZE);
}



void init_monitor(){
	
	prev = calloc(MAX_NUM_TERMINALS, sizeof(struct box));
	curr = calloc(MAX_NUM_TERMINALS, sizeof(struct box));
	next = calloc(MAX_NUM_TERMINALS, sizeof(struct box));   
	tds_ocpd = 0;
	tdscond = CondCreate();
}

void
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
	wt_ocpd[term] = 0;
	rt_ocpd[term] = 0;
	busycond[term] = CondCreate();
	wtcond[term] = CondCreate();
	rtcond[term] = CondCreate();
	inputempty[term] = CondCreate();
	curr[term].initialized = 1;
	next[term].initialized = 1;	
}

extern void
ti_inner(term){
	Declare_Monitor_Entry_Procedure();
	curr[term].tout++;
	busy[term] = 0;
	CondSignal(busycond[term]);
	char c = read_buffer1(echo_buffer[term], term, echobuffer_size, echobuffer_ridx, ECHOSIZE);
	if (c) {
		WriteDataRegister(term, c);
		
		busy[term] = 1;
	}
	return;
}

extern void
ri_inner(term){
	Declare_Monitor_Entry_Procedure();
	char c = ReadDataRegister(term);
	curr[term].tin++;
	if (c == '\r') {
		c = '\n';
	}
	if (c == '\b' || c == '\177') {
		if (input_buffer[term][inputbuffer_widx[term] - 1] == '\n'){}
		else {
			if(inputbuffer_size[term] > 0){
				inputbuffer_size[term]--;
				inputbuffer_widx[term] = (inputbuffer_widx[term] - 1) % SIZE;
			}
			if (busy[term]){
				write_to_buffer1(echo_buffer[term], '\b', term, echobuffer_size, echobuffer_widx, ECHOSIZE);
				write_to_buffer1(echo_buffer[term], ' ', term, echobuffer_size, echobuffer_widx, ECHOSIZE);
				write_to_buffer1(echo_buffer[term], '\b', term, echobuffer_size, echobuffer_widx, ECHOSIZE);
			} else {
				WriteDataRegister(term, '\b');
				busy[term] = 1;
				write_to_buffer1(echo_buffer[term], ' ', term, echobuffer_size, echobuffer_widx, ECHOSIZE);
				write_to_buffer1(echo_buffer[term], '\b', term, echobuffer_size, echobuffer_widx, ECHOSIZE);
			}
		}
		return;
	}
	int temp = write_char_inputbuffer(term, c);
	//if (temp) {CondSignal(inputempty[term]);}
	if (temp) {
		if (busy[term]) {
			if(c == '\n'){
				write_to_buffer1(echo_buffer[term], '\r', term, echobuffer_size, echobuffer_widx, ECHOSIZE);
				write_to_buffer1(echo_buffer[term], '\n', term, echobuffer_size, echobuffer_widx, ECHOSIZE);
			} else {
				write_to_buffer1(echo_buffer[term], c, term, echobuffer_size, echobuffer_widx, ECHOSIZE);
			}
		} else {
			if(c == '\n'){
				WriteDataRegister(term, '\r');
				busy[term] = 1;
				write_to_buffer1(echo_buffer[term], '\n', term, echobuffer_size, echobuffer_widx, ECHOSIZE);
			} else {
				WriteDataRegister(term, c);
				busy[term] = 1;
			}
		}
	}
}
extern int
wt_inner(int term, char *buf, int buflen){
	Declare_Monitor_Entry_Procedure();
	int result = 0;
	char c, cc;
	while(wt_ocpd[term] == 1){CondWait(wtcond[term]);}
	wt_ocpd[term] = 1;
	while (result < buflen){
		while(busy[term]){
			CondWait(busycond[term]);
		}
		c = read_buffer1(echo_buffer[term], term, echobuffer_size, echobuffer_ridx, ECHOSIZE);
	        if (c) {
        	        WriteDataRegister(term, c);                             
                	busy[term] = 1;
			//CondWait(busycond[term]);                                         
        	} else {
			cc = buf[result];
			if (cc == '\n'){
				WriteDataRegister(term, '\r');
				busy[term] = 1;
				write_to_buffer1(echo_buffer[term], '\n', term, echobuffer_size, echobuffer_widx, ECHOSIZE);
			} else {
				WriteDataRegister(term, cc);
				busy[term] = 1;
			}
			
			result++;
			//CondWait(busycond[term]);
		}
	
	}
	CondWait(busycond[term]);
	wt_ocpd[term] = 0;
	CondSignal(wtcond[term]);
	curr[term].uin += result;
	return result;
}

extern int
readterminal_inner(int term, char* buf, int buflen){
	Declare_Monitor_Entry_Procedure();
	int result = 0;
	char c;
	while(rt_ocpd[term] == 1){CondWait(rtcond[term]);}
        rt_ocpd[term] = 1;
	while(result < buflen){
	//	c = read_buffer1(input_buffer[term], term, inputbuffer_size, inputbuffer_ridx, SIZE); 
		while (inputbuffer_size[term] == 0){
			CondWait(inputempty[term]);
		}
		c = read_buffer1(input_buffer[term], term, inputbuffer_size, inputbuffer_ridx, SIZE);
		if(c){

			buf[result] = c;
			result++;
		}
		if (c == '\n') break;
	}
	rt_ocpd[term] = 0;
        CondSignal(rtcond[term]);
	term[curr].uout += result;
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
