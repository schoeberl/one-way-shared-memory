/*
  Software layer for One-Way Shared Memory
  Runs both on the PC and on patmos

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include "onewaysim.h"

#ifdef RUNONPATMOS
//#define ONEWAY_BASE ((volatile _IODEV int *) 0xE8000000)
//volatile _SPM int *alltxmem = ONEWAY_BASE;
//volatile _SPM int *allrxmem = ONEWAY_BASE;
#else
int alltxmem[CORES][TDMSLOTS][WORDS];
int allrxmem[CORES][TDMSLOTS][WORDS];
#endif

static volatile _UNCACHED int testval = -1;
static volatile _UNCACHED int _nextcore = -1;

int nextcore(){
  _nextcore++;
  if (_nextcore == CORES)
    _nextcore = 0;
  return _nextcore;
}

void spinwork(unsigned int waitcycles){
  unsigned int start = getcycles();
  while((getcycles()-start) <= waitcycles);
}

// called by each core thread before starting the state machine
void holdandgowork(int cpuid){
  coreready[cpuid] = true;
  bool allcoresready = false;
  while(!allcoresready){
    allcoresready = true;
    for(int i = 0; i < CORES; i++) 
      if (coreready[i] == false)
        allcoresready = false;
    }
  }

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

// get cpu id from the pointer that was passed to each core thread
  int getcpuidfromptr(void *acpuidptr){
    return *((int*) acpuidptr);
  }

//  saves the current hyperperiod for each TDMSLOT in prevhyperperiod[TDMSLOTS]
//  as seen from each rx core
  void recordhyperperiodwork(int cpuid, unsigned int* hyperperiods){
    for(int i = 0; i < TDMSLOTS; i++){
      hyperperiods[i] = core[cpuid].rx[i][0];
    //sync_printf(cpuid, "prevhyperperiod[i=%d] = 0x%08x (from core %d)\n", 
     //           i, hyperperiods[i], gettxcorefromrxcoreslot(cpuid, i));        
    }
  }






///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Exchange of state
///////////////////////////////////////////////////////////////////////////////
const unsigned int CPUFREQ = 80e6;
void corethreadeswork(void *cpuidptr) {
  int cpuid = *((int*)cpuidptr);
  
  int txcnt = 1;
  const int SENSORID0 = 0x112233;
  const unsigned int SENSORUPDATEHZ = 1;

  es_msg_t esmsg_out;
  es_msg_t esmsg_in[TDMSLOTS];
  
  int state = 0;
  while (runcores) {
    // NOC CONTROL SECTION //
    // overall "noc state" handled by poor core 0 as a sidejob 
    static int roundstate = 0;
    // max state before core 0 stops the mission
    const int MAXROUNDSTATE = 3;
    if (cpuid == 0){
      if(roundstate == MAXROUNDSTATE) {
        // signal to stop the slave cores
        runcores = false; 
        sync_printf(0, "core 0 is done, roundstate == false signalled\n");
      }  
      roundstate++;
    }

    // CORE WORK SECTION //  
    // individual core states incl 0
    switch (state) {
      // tx messages
      case 0: {
        // state work
        if(cpuid == 0){
          sync_printf(cpuid, "core %d to tx sensor state exchange\n", cpuid);
          sync_printf(cpuid, "sizeof(es_msg_t) = %d\n", sizeof(es_msg_t));
        }
        else
          sync_printf(cpuid, "core %d no work in state 0\n", cpuid);
        
        unsigned int sensestart = getcycles();
//        while(getcycles()-sensestart < CPUFREQ/SENSORUPDATEHZ);
        
        // Prepare the sensor reading that is transmitted from core 0 to all the other cores
        if (cpuid == 0) {
          esmsg_out.txstamp   = -1;
          esmsg_out.sensorid  = SENSORID0;
          esmsg_out.sensorval = getcycles(); // the artificial "temperature" proxy
        }

        if (cpuid == 0) { 
          // tx the message
          for(int i=0; i<TDMSLOTS; i++) { 
            core[cpuid].tx[i][0] = cpuid*0x10000000 + i*0x1000000 + 0*10000 + txcnt;
            core[cpuid].tx[i][1] = esmsg_out.sensorid;          
            core[cpuid].tx[i][2] = esmsg_out.sensorval;
          }
          txcnt++;  
        }

        spinwork(TDMSLOTS*WORDS);
        
        // next state
        if (true) {
          state++;
        }
        break;
      }
      // rx sensor  
      case 1: {
        // state work
        // now slave cores receive the message
        if(cpuid != 0){
          sync_printf(cpuid, "core %d es msg rx in state 1\n", cpuid);
          int core0slot = -1;
          for(int i=0; i<TDMSLOTS; i++) 
            if (gettxcorefromrxcoreslot(cpuid, i) == 0)
              core0slot = i;

            esmsg_in[cpuid].txstamp   = core[cpuid].rx[core0slot][0];
            esmsg_in[cpuid].sensorid  = core[cpuid].rx[core0slot][1];          
            esmsg_in[cpuid].sensorval = core[cpuid].rx[core0slot][2];

            sync_printf(cpuid, "esmsg_in[%d](%d) 0x%08x 0x%08x 0x%08x\n",
              core0slot, 0, esmsg_in[cpuid].txstamp, esmsg_in[cpuid].sensorid, 
              esmsg_in[cpuid].sensorval);
          } else {
            sync_printf(cpuid, "core %d no work in this state\n", cpuid);
          }

          if(cpuid != 0){
          // check use case 2 when running on real HW
          #ifdef RUNONPATMOS
            if (esmsg_in[cpuid].sensorid == SENSORID0)
              sync_printf(cpuid, "use case 2 ok (core 0 sensor state to core %d)!\n", cpuid);
            else
              sync_printf(cpuid, "use case 2 not ok (core 0 sensor state to core %d)!\n", cpuid);  
          #endif
          }        
          
          spinwork(TDMSLOTS*WORDS);

        // next state    
          if (true) { 
            state++;
          }
          break;
        }

        default: {
          // no work, just "looping" until runcores == false is signaled from core 0
          break;
        }
      }

    }
  }

///////////////////////////////////////////////////////////////////////////////
//COMMUNICATION PATTERN: Streaming Double Buffer (sdb)
///////////////////////////////////////////////////////////////////////////////
// there is a number of buffers overlaid on the tx and rx memory for each core
// the size of each of these double buffers is DBUFSIZE 

  void corethreadsdbwork(void *cpuidptr)
  {
    int cpuid = *((int*)cpuidptr);
    sync_printf(cpuid, "Core %d started...DOUBLEBUFFERS=%d, TDMSLOTS=%d, DBUFSIZE=%d, WORDS=%d\n", 
      cpuid, DOUBLEBUFFERS, TDMSLOTS, DBUFSIZE, WORDS);
  // setup: each tx and rx tdm slot have two (or more) double buffers
  // each buffer_t represents one (of at least two) for each TDM slot (each 
  //   TDM slot is WORDS long)
  // so for a double buffer on TDM slot 0, there would be two buffer_t stuct objects
  // they look like arrays (of arrays) as written below, but 
  // inside each buffer_t is a pointer to an unsigned int, which is
  // is set up to point to the part of either the rx or tx memory buffer that 
  // a particular buffer_t "controls".

  // double buffers on tx
    buffer_t buf_out[TDMSLOTS][DOUBLEBUFFERS];
  // double buffers on rx
    buffer_t buf_in[TDMSLOTS][DOUBLEBUFFERS];

  // init the data pointer in all of the double buffers
  // it will print something like this:
  //   [cy592233042 id00 #01] buf_out[0][0].data address = 0xe8000000
  //   [cy592305786 id00 #02] buf_in[0][0].data address  = 0xe8000000
  //   as the tx and rx are memory mapped to the same address offset (ONEWAY_BASE)
    for(int i=0; i<TDMSLOTS; i++){
      for(int j=0; j < DOUBLEBUFFERS; j++){
        buf_out[i][j].data = (volatile _IODEV int *) (core[cpuid].tx[i] + (j*DBUFSIZE));  
      //sync_printf(cpuid, "&core[cpuid].tx[i][1] = %p, (j*DBUFSIZE) = %d\n",
      //                 &core[cpuid].tx[i][1], (j*DBUFSIZE));
        sync_printf(cpuid, "&buf_out[tdm=%d][dbuf=%d].data[1] address = %p\n", 
         i, j, &buf_out[i][j].data[1]);     
        for(int k=0; k < DBUFSIZE; k++){
          buf_out[i][j].data[k] = 0;
        }
      }    
    }

    for(int i=0; i<TDMSLOTS; i++){
      for(int j=0; j < DOUBLEBUFFERS; j++){
        buf_in[i][j].data = (volatile _IODEV int *) (core[cpuid].rx[i] + (j*DBUFSIZE));   
        sync_printf(cpuid, "buf_in[%d][%d].data address  = %p\n", 
          i, j, buf_in[i][j].data);   
      }
    }

    sync_printf(cpuid, "core %d mission start on cycle %d\n", cpuid, getcycles());
  //if (cpuid !=0) 
  //  holdandgowork(cpuid);

    int txcnt = 1;
    int state = 0;
  // used by core 0
    int roundstate = 0;
    const int MAXROUNDSTATE = 1;
    if(cpuid == 0) state++;

    do {  
    // CORE WORK SECTION //  
    // individual core states incl 0

      switch (state) {
      // all tx their buffers
        case 0: {
        // UNCOMMENT THIS WHEN NOT DOING MEASUREMENTS
        //sync_printf(cpuid, "core %d to tx it's buffer in state 0\n", cpuid);

        //spinwork(TDMSLOTS*WORDS);
        //spinwork(TDMSLOTS*WORDS);  
        // first round of tx for the first buffer
          for(int i=0; i<TDMSLOTS; i++) {
          //buf_out[i][0].data[0] = cpuid*0x10000000 + i*0x1000000 + 0*10000 + txcnt; 
            for(int j=0; j < DBUFSIZE; j++){
              buf_out[i][0].data[j] = 2*cpuid*0x10000000 + i*0x1000000 + j*10000 + txcnt;
            }
          //sync_printf(cpuid, 
          //  "buf_out[%d](%d->).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
          //  i, getrxcorefromtxcoreslot(cpuid, i), buf_out[i][0].data[0], 
          //  buf_out[i][0].data[1], DBUFSIZE-1, buf_out[i][0].data[DBUFSIZE-1]);
          }
          txcnt++;  

        // second round of tx for the second buffer
          for(int i=0; i<TDMSLOTS; i++) {
          //buf_out[i][1].data[0] = cpuid*0x10000000 + i*0x1000000 + 0*10000 + txcnt; 
            for(int j=0; j < DBUFSIZE; j++){
              buf_out[i][1].data[j] = cpuid*0x10000000 + i*0x1000000 + j*10000 + txcnt;
            }
          //sync_printf(cpuid, 
          //  "buf_out[%d](%d->).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
          //  i, getrxcorefromtxcoreslot(cpuid, i), buf_out[i][1].data[0], 
          //  buf_out[i][1].data[1], DBUFSIZE-1, buf_out[i][1].data[DBUFSIZE-1]);
          }
          txcnt++;  

        // next state
          if (true) {
          //state++;
          }
          break;
        }
      // rx buffer core 0  
        case 1: {
        // state work
          sync_printf(cpuid, "core %d state 1 on cycle %d\n", cpuid, getcycles());
        //if (cpuid == 0) holdandgowork(cpuid);
          const int num = 5;

          int cyclestamp[DOUBLEBUFFERS][num];
          int lastword[DOUBLEBUFFERS][num];
          int aword = -1;
          volatile int tmpsum = 0;
        // Now for real-time measurements between core 1 and core 0
        //   remember to comment out the sync_print line that is the first statement for
        //   state case 0 in the switch
        const int rxslot = 2; // just pick one
        const int txcoreid = gettxcorefromrxcoreslot(cpuid, rxslot);
        // start by monitoring the last word of the first buffer
        aword = buf_in[rxslot][0].data[DBUFSIZE-1];
        for(int i=0; i < num; i++) {
          for(int j=0; j < DOUBLEBUFFERS; j++){
//            while(aword == buf_in[rxslot][j].data[DBUFSIZE-1]);
            aword = buf_in[rxslot][j].data[DBUFSIZE-1];
            cyclestamp[j][i] = getcycles();
            // core 1 is in TDM slot 2
            lastword[j][i] = buf_in[rxslot][j].data[DBUFSIZE-1];
            // some actual work use the active buffer (summing it as an example)
            for(int k=0; k < DBUFSIZE; k++)
              tmpsum += buf_in[rxslot][j].data[k];
          }
        }
        // capture the real end-time. The last 'cyclestamp' is taken before the 
        //   buffer is used and would report a too small number
        int endtime = getcycles();
        // done with real-time stuff. The rest is for "fun". 
        for(int i=0; i < num; i++){
          for(int j=0; j < DOUBLEBUFFERS; j++){
            sync_printf(cpuid, "double buffer %d: cyclestamp[%d] = %d, lastword[%d](1->) = 0x%08x\n",
              j, i, cyclestamp[j][i], i, lastword[j][i]);
          }
        }
        int totalcycles = endtime - cyclestamp[0][0];
        sync_printf(cpuid, "total cycles for %d rounds: %d, average = %d\n",
         num, totalcycles, totalcycles/num);

        if(cpuid == 0){
          sync_printf(cpuid, "core 0 in double buffer 0 rx state 1\n", cpuid);
          for(int i=0; i<TDMSLOTS; i++) {
            sync_printf(cpuid, 
              "buf_in[%d](->%d).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
              i, gettxcorefromrxcoreslot(cpuid, i), buf_in[i][0].data[0], 
              buf_in[i][0].data[1], DBUFSIZE-1, buf_in[i][0].data[DBUFSIZE-1]);
          }

          sync_printf(cpuid, "core 0 in double buffer 1 rx state 1\n", cpuid);
          for(int i=0; i<TDMSLOTS; i++) {
            sync_printf(cpuid, 
              "buf_in[%d](->%d).data[0]=0x%08x : .data[1]=0x%08x ... .data[%d]=0x%08x\n",
              i, gettxcorefromrxcoreslot(cpuid, i), buf_in[i][1].data[0], 
              buf_in[i][1].data[1], DBUFSIZE-1, buf_in[i][1].data[DBUFSIZE-1]);
          }

          // check rough timing
          if(totalcycles < 1e6)
            sync_printf(cpuid, "use case 3 ok!\n", cpuid);
          else
            sync_printf(cpuid, "use case 3 not ok!\n", cpuid);

        } else {
          sync_printf(cpuid, "core %d have no rx work in state 1\n", cpuid);
        }

        spinwork(TDMSLOTS*WORDS);

        // next state    
        if (true) { 
          //state++;
        }
        break;
      }

      default: {
          // no work, just "looping" until runcores == false is signaled from core 0
        break;
      }
    }
    // NOC CONTROL SECTION BY CORE 0//
    // max state before core 0 stops the mission
    if (cpuid == 0){
      roundstate++;
      if(roundstate >= MAXROUNDSTATE) {
        // signal to stop the slave cores (and core 0)
        runcores = false; 
        sync_printf(0, "core 0 is done, roundstate == false signalled\n");
        //printf("core 0 is done, roundstate == false signalled\n");
      }  
    } 
  } while (runcores);
}

///////////////////////////////////////////////////////////////////////////////
// Control code for communication patterns
///////////////////////////////////////////////////////////////////////////////

#ifdef RUNONPATMOS
//static corethread_t corethread[CORES];
#else
static pthread_t corethread[CORES];
#endif

///////////////////////////////////////////////////////////////////
// utility stuff
///////////////////////////////////////////////////////////////////

// mappings
static const char *rstr = ROUTESSTRING;

static int tx_core_tdmslots_map[CORES][TDMSLOTS];
static int rx_core_tdmslots_map[CORES][TDMSLOTS];

// route strings
static char *routes[TDMSLOTS];

// get coreid from NoC grid position
int getcoreid(int row, int col, int n){
  return row * n + col;
}

// convert the routes string into separate routes 
void initroutestrings(){
  printf("initroutestrings:\n");
  int start = 0;
  int stop = 0;
  for(int i = 0; i < TDMSLOTS; i++){
    while(rstr[stop] != '|')
      stop++;
    routes[i] = calloc(stop - start + 1 , 1);
    strncpy(routes[i], &rstr[start], stop - start);
    stop++;
    start = stop;
    printf("%s\n", routes[i]);
  }
}

// init rxslot and txcoreid lookup tables
void txrxmapsinit()
{
  initroutestrings();
  // tx router grid:
  //   the idea here is that we follow the word/flit from its tx slot to its rx slot.
  //   this is done by tracking the grid index for each of the possible directins of
  //   'n', 'e', 's', and 'w'. From the destination grid position the core id is 
  //   calculated (using getcoreid(gridx, gridy, n). 
  for(int tx_i = 0; tx_i < GRIDN; tx_i++){
    for(int tx_j = 0; tx_j < GRIDN; tx_j++){
      // simulate each route for the given rx core
      for(int slot = 0; slot < TDMSLOTS; slot++){
	// the tx and rx tdm slot are known by now
        char *route = routes[slot];

        int txcoreid = getcoreid(tx_i, tx_j, GRIDN);
        int rxcoreid = -1;

        int txtdmslot = slot;
        int rxtdmslot = (strlen(route)) % 3; 

	// now for the rx core id
        int rx_i = tx_i;
        int rx_j = tx_j;
        for(int r = 0; r < strlen(route); r++){
          switch(route[r]){
           case 'n':
           rx_i = (rx_i - 1 >= 0 ? rx_i - 1 : GRIDN - 1);
           break;
           case 's':
           rx_i = (rx_i + 1 < GRIDN ? rx_i + 1 : 0);
           break;
           case 'e':
           rx_j = (rx_j + 1 < GRIDN ? rx_j + 1 : 0);
           break;
           case 'w':
           rx_j = (rx_j - 1 >= 0 ? rx_j - 1 : GRIDN - 1);
           break;
         }
       }        

       rxcoreid = getcoreid(rx_i, rx_j, GRIDN);

        // fill in the rx core id in the tx slot map
       tx_core_tdmslots_map[txcoreid][txtdmslot] = rxcoreid;
	    // fill in the tx core id in the rx slot map
       rx_core_tdmslots_map[rxcoreid][rxtdmslot] = txcoreid;
     }
   }
 }
}

// will print the TX and RX TDM slots for each core
void showmappings(){
  printf("Transmit memory blocks and receive memory blocks (see Fig. 3 in the paper):\n");
  for(int i = 0; i < CORES; i++){
    printf("  Core %d tdm slots:\n", i);
    // show them like in the paper
    for(int j = TDMSLOTS-1; j >= 0; j--){
      printf("    tx slot %d:   to rx core %d rx slot %d\n", 
             j, getrxcorefromtxcoreslot(i, j), //tx_core_tdmslots_map[i][j],
             getrxslotfromrxcoretxcoreslot(tx_core_tdmslots_map[i][j], i, j));//getrxcorefromtxcoreslot(i, j)); 
    }
    for(int j = TDMSLOTS-1; j >= 0; j--){
      printf("    rx slot %d: from tx core %d tx slot %d\n", 
             j, gettxcorefromrxcoreslot(i, j), //rx_core_tdmslots_map[i][j],
             gettxslotfromtxcorerxcoreslot(rx_core_tdmslots_map[i][j], i, j));//gettxcorefromrxcoreslot(i, j)); 
    }
  }
}

// get the rx tdm slot based on rx core, tx core and tx (TDM) slot index
int getrxslotfromrxcoretxcoreslot(int rxcore, int txcore, int txslot){
  int rxslot = -1;
  for(int i = 0; i < TDMSLOTS; i++){
    int txcorecandidate = gettxcorefromrxcoreslot(rxcore, i);  
    if (txcorecandidate == txcore){
      rxslot = i;
      break;
    }
  }
  return rxslot;
}

// get the tx tdm slot based on tx core, rx core and rx (TDM) slot index
int gettxslotfromtxcorerxcoreslot(int txcore, int rxcore, int rxslot){
  int txslot = -1;
  for(int i = 0; i < TDMSLOTS; i++){
    int rxcorecandidate = getrxcorefromtxcoreslot(txcore, i);  
    if (rxcorecandidate == rxcore){
      txslot = i;
      break;
    }
  }
  return txslot;
}

// get the rx core based on tx core and tx (TDM) slot index
int getrxcorefromtxcoreslot(int txcore, int txslot){
  return tx_core_tdmslots_map[txcore][txslot];
}

// get the tx core based on rx core and rx (TDM) slot index
int gettxcorefromrxcoreslot(int rxcore, int rxslot){
  return rx_core_tdmslots_map[rxcore][rxslot];
}
