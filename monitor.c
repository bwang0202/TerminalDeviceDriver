#include <stdio.h>
#include <stdlib.h>
#include <hardware.h>
#include <threads.h>

#define SIZE 200
#define ECHOSIZE 200
//struct for storing terminal statistics.
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
//input_buffer size
static int inputbuffer_size[MAX_NUM_TERMINALS - 1];
//input_buffer read index
static int inputbuffer_ridx[MAX_NUM_TERMINALS - 1];
//input_buffer write index
static int inputbuffer_widx[MAX_NUM_TERMINALS - 1];
//each ehco_buffer: user threads could read from this buffer as well as
//main terminal thread.
static char *echo_buffer[MAX_NUM_TERMINALS - 1];
//echo_buffer size
static int echobuffer_size[MAX_NUM_TERMINALS - 1];
//echo_buffer read index
static int echobuffer_ridx[MAX_NUM_TERMINALS - 1];
//echo_buffer write index
static int echobuffer_widx[MAX_NUM_TERMINALS - 1];
//represents if a terminal is busy displaying last character. This data
//is set and read by user threads and main thread.
static int busy[MAX_NUM_TERMINALS - 1];
//Conditional variable coordinating WriteDataRegister to hardware.
static cond_id_t busycond[MAX_NUM_TERMINALS - 1];
//Read and set by user threads. Representing if there exists some write_terminal
//not returning.
static int wt_ocpd[MAX_NUM_TERMINALS - 1];
//Conditional variable making sure only one thread is in write_terminal procedure
//at a given time.
static cond_id_t wtcond[MAX_NUM_TERMINALS - 1];
//Read and set by user threads. Representing if there exists some read_terminal
//not returning.
static int rt_ocpd[MAX_NUM_TERMINALS - 1];
//Conditional variable making sure only one thread is in read_terminal procedure.
static cond_id_t rtcond[MAX_NUM_TERMINALS - 1];
//read and set by multiple user thread. representing if there exists some terminal_
//statistics calls not returning.
static int tds_ocpd;
//conditional variable making sure only one thread is calling terminal statistics method.
static cond_id_t tdscond;
//Conditional variable coordinating read and write to input_buffer.
static cond_id_t inputempty[MAX_NUM_TERMINALS - 1];
//a list of previous copy of terminal statistics.
static struct box *prev;
//a list of current copy of terminal statistics.
static struct box *curr;
//an allocated memory location to store future terminal statistics.
static struct box *next;

/**
 * Entry procedure: return all terminal statistics into allocated space pointed by stat.
 */
extern void
tds(struct termstat* stat){
	Declare_Monitor_Entry_Procedure();
	struct box *old;
	int i;
	while(tds_ocpd){
		CondWait(tdscond);
	}
	tds_ocpd = 1;
	//take a snap image of current terminal statistics.
	//after this pointer assignment, all new data will go into "next".
	//Then we can slowly copy the snapped statistics into buffer without anyone ever
	//changing it.
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
	free(old); //make sure memory usage is bounded.
	next = calloc(MAX_NUM_TERMINALS, sizeof(struct box));
	tds_ocpd = 0;
	CondSignal(tdscond);
	return;	
}
/**
 * convenience method to write a character to buffer.
 * returns number of character written.
 */
static
int write_to_buffer1(char *buffer, char c, int i, int *buffer_size, int* widx, int max) {
        if(buffer_size[i] == max) return 0;
        buffer[widx[i]] = c;
        widx[i] = (widx[i] + 1) % max;
        buffer_size[i]++;
        return 1;
}
/**
 * convenience method to read a character to buffer.
 * return the character if the buffer is not empty. return 0 if the buffer is empty.
 * This does not block.
 */
static
char read_buffer1(char *buffer, int i, int *buffer_size, int* ridx, int max) { 
        char c;
        if(buffer_size[i] == 0) return 0;
        c = buffer[ridx[i]];
        ridx[i] = (ridx[i] + 1) % max;
        buffer_size[i]--;
        return c;
}
/**
 * convenience method to put character into input_buffer as well as setting conditional variable
 * correctly.
 */
static int
write_char_inputbuffer(int term, char c){
       	CondSignal(inputempty[term]);
        return write_to_buffer1(input_buffer[term], c, term, inputbuffer_size, inputbuffer_widx, SIZE);
}


/**
 * Initialize monitor by allocating prev, curr, next for storing terminal statistics.
 * At any given time, due to the free() function call in the tds(), the memory usage is thus bounded.
 */
void init_monitor(){
	
	prev = calloc(MAX_NUM_TERMINALS, sizeof(struct box));
	curr = calloc(MAX_NUM_TERMINALS, sizeof(struct box));
	next = calloc(MAX_NUM_TERMINALS, sizeof(struct box));   
	//initialize cond var
	tds_ocpd = 0;
	tdscond = CondCreate();
}
/**
 * Initialize a terminal by initializing its input, echo buffers and cond variables.
 */
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
/**
 * Entrance procedure for transmit interrupt handler.
 * If echo is not empty, then continue to write to terminal.
 * Output buffer is not taken care here, but in the other procedure for write terminal
 * as well as giving echo buffer priority.
 */
extern void
ti_inner(term){
	Declare_Monitor_Entry_Procedure();
	curr[term].tout++;
	busy[term] = 0;
	CondSignal(busycond[term]);
	char c = read_buffer1(echo_buffer[term], term, echobuffer_size, echobuffer_ridx, ECHOSIZE);
	if (c) {  //if echo buffer is not empty
		WriteDataRegister(term, c);
		
		busy[term] = 1;
	}
	return;
}
/**
 * Entry procedure for receive interrupt.
 */
extern void
ri_inner(term){
	Declare_Monitor_Entry_Procedure();
	char c = ReadDataRegister(term);
	curr[term].tin++; //update statistics
	//character processing
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
	// successufully wrote into inputbuffer.	
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
/**
 * Entry procedure for write terminal.
 * Use cond var to avoid threads interleaving.
 */
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
		//give priority to echo buffer.
		c = read_buffer1(echo_buffer[term], term, echobuffer_size, echobuffer_ridx, ECHOSIZE);
	        if (c) {
        	        WriteDataRegister(term, c);                             
                	busy[term] = 1;
			                                         
        	} else {
			cc = buf[result];
			//character processing.
			if (cc == '\n'){
				WriteDataRegister(term, '\r');
				busy[term] = 1;
				write_to_buffer1(echo_buffer[term], '\n', term, echobuffer_size, echobuffer_widx, ECHOSIZE);
			} else {
				WriteDataRegister(term, cc);
				busy[term] = 1;
			}
			
			result++;
			
		}
	
	}
	CondWait(busycond[term]); //make sure last character is displayed.
	wt_ocpd[term] = 0;
	CondSignal(wtcond[term]);
	curr[term].uin += result;
	return result;
}
/**
 * Entry procedure for read terminal.
 * Uses cond var to avoid threads interleaving.
 */
extern int
readterminal_inner(int term, char* buf, int buflen){
	Declare_Monitor_Entry_Procedure();
	int result = 0;
	char c;
	while(rt_ocpd[term] == 1){CondWait(rtcond[term]);}
        rt_ocpd[term] = 1;
	while(result < buflen){
 
		while (inputbuffer_size[term] == 0){
			CondWait(inputempty[term]);
		}
		c = read_buffer1(input_buffer[term], term, inputbuffer_size, inputbuffer_ridx, SIZE);
		if(c){

			buf[result] = c;
			result++;
		}
		if (c == '\n') break; //immediately returns if hitting new line.
	}
	rt_ocpd[term] = 0;
        CondSignal(rtcond[term]);
	term[curr].uout += result;
	return result;
}
                              
