//syncprint.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include <math.h>

#ifndef RUNONPATMOS
#include <time.h>
#endif
#include "syncprint.h"


//print support so the threads on all cores can use printf
//  call sync_printall to print them after the most
//  important code has run.
//  don't not use sync_printf when running cycle accurate code (e.g. wcet)
static char strings[PRINTCORES][SYNCPRINTBUF][LINECHARS];
// clock cycles
static unsigned long timestamps[PRINTCORES][SYNCPRINTBUF];
// message counter per core
static int mi[PRINTCORES];

// make sync_printf and info_printf thread safe
static volatile _UNCACHED int printtoken = -1;
static volatile _UNCACHED bool firsttime = true;

// used for synchronizing printf from the different cores
int getcycles()
{
#ifdef RUNONPATMOS
  volatile _IODEV int *io_ptr = (volatile _IODEV int *)0xf0020004; 
  return (unsigned int)*io_ptr;
#else
  clock_t now_t;
  now_t = clock();
  return (int)now_t;
  //unsigned long a, d;
  //__asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
  //return (int)a;
#endif
}

// call it with the core id so there are no race conditions
void sync_printf(int cid, const char *format, ...)
{
  //uncomment next line to run with shared mutex
  while(printtoken != -1){};
  
  printtoken = cid;
  if (mi[cid] < SYNCPRINTBUF)
  {
    int val = 0;
    timestamps[cid][mi[cid]] = getcycles(); //(unsigned long)val;
    va_list args;
    va_start(args, format);
    vsprintf(&strings[cid][mi[cid]][0], format, args);
 //printf(format, args);
    //printf("cid %d: %s\n", cid, strings[cid][mi[cid]]);
    va_end(args);
    mi[cid]++;
  }
  printtoken = -1;
}



// print minimum timestamp. go over the [core][msg] matrix and find the lowest timestamp
// then print that message. keep a current msg index for each counter. increase it
//   for the message that has just been printed.

// call this from 0 when done
void sync_printall()
{
  bool print = true;
  int closestcoreid = 0;
  int closestmsgid = 0;
  int minmark[PRINTCORES]; // keep track of messages printed for each core

  for (int c = 0; c < PRINTCORES; c++)
    minmark[c] = 0;

  while (print)
  {
    unsigned long smallest = 0xffffffff;
    for (int c = 0; c < PRINTCORES; c++)
    {
      if (minmark[c] < mi[c])
      {
        if (timestamps[c][minmark[c]] <= smallest)
        {
          closestcoreid = c;                    // candidate
          smallest = timestamps[c][minmark[c]]; // new smallest
        }
      }
    }
    printf("[cycle=%'lu, core=%d, msg#=%2d] %s", 
           timestamps[closestcoreid][minmark[closestcoreid]], closestcoreid,
           minmark[closestcoreid], strings[closestcoreid][minmark[closestcoreid]]);
    
    minmark[closestcoreid]++;
    print = false;
    for (int c = 0; c < PRINTCORES; c++)
    {
      // are we done?
      if (minmark[c] < mi[c])
      {
        print = true;
        break;
      }
    }
  }

  //printf("leaving sync_printall()...\n");
}


// print for one core
// infocore is id = PRINTCORES (same as CORES + 1)
void sync_print_core(int id)
{
  setlocale(LC_ALL,"");
  bool print = true;
  int closestcoreid = 0;
  int closestmsgid = 0;
  int minmark[PRINTCORES]; // keep track of messages printed for each core

  for (int c = 0; c < PRINTCORES; c++)
    minmark[c] = 0;

  while (print)
  {
    unsigned long smallest = 0xffffffff;
    for (int c = 0; c < PRINTCORES; c++)
    {
      if (minmark[c] < mi[c])
      {
        if (timestamps[c][minmark[c]] <= smallest)
        {
          closestcoreid = c;                    // candidate
          smallest = timestamps[c][minmark[c]]; // new smallest
        }
      }
    }
    if (closestcoreid == id){
      printf("[cyc%d cpuid%02d #%02d] %s", 
             (int)timestamps[closestcoreid][minmark[closestcoreid]], 
             closestcoreid, minmark[closestcoreid], 
             strings[closestcoreid][minmark[closestcoreid]]);
    }

    minmark[closestcoreid]++;
    print = false;
    for (int c = 0; c < PRINTCORES; c++)
    {
      // done?
      if (minmark[c] < mi[c])
      {
        print = true;
        break;
      }
    }
  }
}
