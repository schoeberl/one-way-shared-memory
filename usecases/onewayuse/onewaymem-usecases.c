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

//static volatile _UNCACHED int testval = -1;
static volatile _UNCACHED int _nextcore = -1;

// called each time the control loop "hits" the default state for a core
void defaultstatework(State** state, int cpuid){
  if (!(*state)->coredone){
    (*state)->coredone = true;
    (*state)->runcore = false; 
    alldone(cpuid);
    sync_printf(cpuid, "core %d done (default final state): use case ok\n", cpuid);
  }
  if (cpuid == 0){
    // when all cores are done (i.e., 'default' state) then signal to 
    // the other cores 1..CORES-1 to stop using the global flag 'runcores'
    if (alldone(cpuid))
      runcores = false;
  }
}

void timeoutcheckcore0(State** state){
  if((*state)->loopcount == 1e6) {
    (*state)->runcore = false; 
    // signal to stop the slave cores
    runcores = false;
    sync_printf(0, "core 0:: roundstate == false signalled: use case not ok\n");
  } 
}

int nextcore() {
  _nextcore++;
  if (_nextcore == CORES)
    _nextcore = 0;
  return _nextcore;
}

void spinwork(unsigned int waitcycles) {
  unsigned int start = getcycles();
  while((getcycles()-start) <= waitcycles);
}

void zeroouttxmem(int cpuid) {
  for (int w = 0; w < WORDS; w++) {
    for (int i = 0; i < TDMSLOTS; i++) {
      core[cpuid].tx[i][w] = 0x00000000;
    }
  } 
}

// called by each core thread before starting the state machine
// blocks, so only use on real HW with independent running cores
void holdandgowork(int cpuid) {
  zeroouttxmem(cpuid);
  spinwork(1e6);
  coreready[cpuid] = true;
  bool allcoresready = false;
  while(!allcoresready){
    allcoresready = true;
    for(int i = 0; i < CORES; i++) 
      if (coreready[i] == false)
        allcoresready = false;
  }
}

// called by each core when they reach the final 'default' state
// the use-cases are set such that reaching this final state is a success
// so we must wait for all cores to finish before core 0 signals to the 
// other cores to stop using 'runcores = false'
bool alldone(int cpuid) {
  coredone[cpuid] = true;
  bool allcoresdone = true;
  for(int i = 0; i < CORES; i++) 
    allcoresdone = allcoresdone && coredone[i];
  
  return allcoresdone;
}

// call this at the end to check final result
const char* allfinishedok() {
  bool allcoresfinishedok = true;
  for(int i = 0; i < CORES; i++) 
    allcoresfinishedok = allcoresfinishedok && coredone[i];
  
  return (allcoresfinishedok ? "pass" : "fail");
}

// used for synchronizing printf from the different cores
int getcycles() {
#ifdef RUNONPATMOS
  volatile _IODEV int *io_ptr = (volatile _IODEV int *)0xf0020004; 
  return (unsigned int)*io_ptr;
#else
  clock_t now_t;
  now_t = clock();
  return (int)now_t;
  //This was real clock cycles on Ubunty
  //unsigned long a, d;
  //__asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
  //return (int)a;
#endif
}

// get cpu id from the pointer that was passed to each core thread
int getcpuidfromptr(void *acpuidptr) {
  return *((int*) acpuidptr);
}

//  saves the current hyperperiod for each TDMSLOT in prevhyperperiod[TDMSLOTS]
//  as seen from each rx core
void recordhyperperiodwork(int cpuid, unsigned int* hyperperiods) {
  for(int i = 0; i < TDMSLOTS; i++){
    hyperperiods[i] = core[cpuid].rx[i][0];
  //sync_printf(cpuid, "prevhyperperiod[i=%d] = 0x%08x (from core %d)\n", 
   //           i, hyperperiods[i], gettxcorefromrxcoreslot(cpuid, i));        
  }
}

// mappings
static const char *rstr = ROUTESSTRING;
static int tx_core_tdmslots_map[CORES][TDMSLOTS];
static int rx_core_tdmslots_map[CORES][TDMSLOTS];
// route strings
static char *routes[TDMSLOTS];

// get coreid from NoC grid position
int getcoreid(int row, int col, int n) {
  return row * n + col;
}

// convert the routes string into separate routes 
void initroutestrings() {
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
void txrxmapsinit() {
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
void showmappings() {
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
int getrxslotfromrxcoretxcoreslot(int rxcore, int txcore, int txslot) {
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
int gettxslotfromtxcorerxcoreslot(int txcore, int rxcore, int rxslot) {
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
int getrxcorefromtxcoreslot(int txcore, int txslot) {
  return tx_core_tdmslots_map[txcore][txslot];
}

// get the tx core based on rx core and rx (TDM) slot index
int gettxcorefromrxcoreslot(int rxcore, int rxslot) {
  return rx_core_tdmslots_map[rxcore][rxslot];
}
