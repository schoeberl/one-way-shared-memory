#ifndef SYNCPRINT_H
#define SYNCPRINT_H
#include <math.h>

#ifdef __patmos__
#include "libcorethread/corethread.h"
#else
#include <pthread.h>
#endif

// The utility consists of three functions. One function enables collection
// of print messages (lines) while the system is running. For information like
// information messages related to overall status there is also a function. 
// Finally, there is a function for printing the messages, which sorts them in 
// the sequence that they were stored while the system was running. 

// Configuration:
// How many cores need printing
#define PRINTCORES 4
// How many lines the printfbuffer can store for each core (0, 1, 2, ...)
//   and one "extra for info-printing on core 0
#define SYNCPRINTBUF 600
// configure max. chars per line
#define LINECHARS    120
 
// Example: sync_printf(cid, "Core %d got new buffer...\n", cid);
void sync_printf(int, const char *format, ...);
// Example: info_printf("core thread %d joined\n", cid)
//void info_printf(const char *format, ...);
// Call from core 0 after all threads have joined and the "mission" is over
void sync_printall();
// Printing just for one core at a time
void sync_print_core(int id);
// get cycles (patmos) or time (pc)
unsigned int getcycles();
#endif

