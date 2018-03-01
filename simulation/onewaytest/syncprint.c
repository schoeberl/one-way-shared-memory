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
static char strings[PRINTCORES + 1][SYNCPRINTBUF][LINECHARS];
// clock cycles
static unsigned long timestamps[PRINTCORES + 1][SYNCPRINTBUF];
// message counter per core
static int mi[PRINTCORES + 1];

// used for synchronizing printf from the different cores
unsigned int getcycles()
{
#ifdef RUNONPATMOS
  volatile _IODEV int *io_ptr = (volatile _IODEV int *)0xf0020004; 
  return (unsigned int)*io_ptr;
#else
  clock_t now_t;
  now_t = clock();
  return (unsigned int)now_t;
#endif
}
	
// call it with the core id so there are no race conditions
void sync_printf(int cid, const char *format, ...)
{
  if (mi[cid] < SYNCPRINTBUF)
  {
    int val = 0;
    timestamps[cid][mi[cid]] = getcycles(); //(unsigned long)val;
    va_list args;
    va_start(args, format);
    vsprintf(&strings[cid][mi[cid]][0], format, args);
    va_end(args);
    mi[cid]++;
  }
}

// an extra print buffer for info and end of sim stuff
void info_printf(const char *format, ...)
{
  // cid is equal to the number of CORES (i.e., the top buffer)
  if (mi[PRINTCORES] < SYNCPRINTBUF)
  {
    timestamps[PRINTCORES][mi[PRINTCORES]] = getcycles(); //(unsigned long)val;
    va_list args;
    va_start(args, format);
    vsprintf(&strings[PRINTCORES][mi[PRINTCORES]][0], format, args);
    va_end(args);
    mi[PRINTCORES]++;
  }
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
  int minmark[PRINTCORES + 1]; // keep track of messages printed for each core

  for (int c = 0; c < PRINTCORES + 1; c++)
    minmark[c] = 0;

  while (print)
  {
    unsigned long smallest = 0xffffffff;
    for (int c = 0; c < PRINTCORES + 1; c++)
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
    if (closestcoreid == PRINTCORES) // info core?
      printf("[cycle=%lu, info=%d, msg#=%2d] %s", timestamps[closestcoreid][minmark[closestcoreid]], 0,
             minmark[closestcoreid], strings[closestcoreid][minmark[closestcoreid]]);
    else // other cores
      printf("[cycle=%lu, core=%d, msg#=%2d] %s", timestamps[closestcoreid][minmark[closestcoreid]], closestcoreid,
             minmark[closestcoreid], strings[closestcoreid][minmark[closestcoreid]]);
    
    // perhaps not needed, but afraid of overflowing uart buffer?
    usleep(1000);
    
    minmark[closestcoreid]++;
    print = false;
    for (int c = 0; c < PRINTCORES + 1; c++)
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
  bool print = true;
  int closestcoreid = 0;
  int closestmsgid = 0;
  int minmark[PRINTCORES + 1]; // keep track of messages printed for each core

  for (int c = 0; c < PRINTCORES + 1; c++)
    minmark[c] = 0;

  while (print)
  {
    unsigned long smallest = 0xffffffff;
    for (int c = 0; c < PRINTCORES + 1; c++)
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
      if (closestcoreid == PRINTCORES) // info core?
        printf("[cycle=%lu, info=%d, msg#=%2d] %s", timestamps[closestcoreid][minmark[closestcoreid]], 0,
               minmark[closestcoreid], strings[closestcoreid][minmark[closestcoreid]]);
      else // other cores
        printf("[cycle=%lu, core=%d, msg#=%2d] %s", timestamps[closestcoreid][minmark[closestcoreid]], 
               closestcoreid, minmark[closestcoreid], strings[closestcoreid][minmark[closestcoreid]]);
    }
    // perhaps not needed, but afraid of overflowing uart buffer?
    usleep(1000);
    
    minmark[closestcoreid]++;
    print = false;
    for (int c = 0; c < PRINTCORES + 1; c++)
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


