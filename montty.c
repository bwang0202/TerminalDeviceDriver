#include <stdio.h>
#include <stdlib.h>
#include <hardware.h>
#include <terminals.h>
#include "monitor.c"
/**
 * Recieve interrupt handler.
 * Calls an entry procedure in monitor.c the "Monitor".
 * But this will not block in any way because only the main thread will receive this interrupt.
 */
void ReceiveInterrupt(int term){
	return ri_inner(term);
}
/**
 * Transmit interrupt handler.
 * Calls entry procedure in monitor.c the "Monitor".
 */
void TransmitInterrupt(int term){
	return ti_inner(term);
}
/**
 * User function: write buflen of characters from buf to terminal term.
 * Return the number of characters sucessfully transferred. Return -1
 * on error.
 * It calls entry procedure in monitor to write.
 */
int WriteTerminal(int term, char *buf, int buflen){
	//initialize output buffer.
	char *output_buffer = buf;
	int output_len = buflen;
	return wt_inner(term, output_buffer, output_len);
}
/**
 * User function: read from terminal term into buf, for buflen if not
 * encountering new line. Return number of characters transferred. 
 * Return -1 on error.
 *
 */
int ReadTerminal(int term, char *buf, int buflen){
	char *buffer = buf;
	int len = buflen;
	return readterminal_inner(term, buffer, len);
}
/**
 * Initialize terminal term by calling initHareware and init monitor.
 */
int InitTerminal(int term){
	InitHardware(term);
	init_moniterminal(term);	
	return 0;
}
/**
 * Put statistics of all terminals into allocated location pointed by
 * stat.
 */
int TerminalDriverStatistics(struct termstat *stat){
	struct termstat* t = stat;
	tds(t);
	return 0;
}
/**
 * Initialize this driver, by calling init monitor.
 */
int InitTerminalDriver(){
	init_monitor();
	return 0;
}














