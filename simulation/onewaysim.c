/*
  A software simulation of the One-Way Shared Memeory

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// start with hard coded 4 cores for a quick start


unsigned long alltxmem[4][1000];
unsigned long allrxmem[4][1000];

// void *core(void *vargp) {
// the following is wrong, but I'm too lazy to find  the right casting
void *core(int *vargp) {

  int id = (int) *vargp;
  unsigned long *txmem;
  txmem = alltxmem[id];
  unsigned long *rxmem;
  rxmem = allrxmem[id];

  printf("I am core %d\n", id);
  // this should be the work done
  for (int i=0; i<10; ++i) {
    printf("%d: %d, ", id, allrxmem[id][0]);
    fflush(stdout);
    alltxmem[id][0] = id*10 + id;
    sleep(1);
  }
  return NULL;
}
 
int main() {

  pthread_t tid[4];
  int id[4];
  printf("Simulation starts\n");
  for (int i=0; i<4; ++i) {
    id[i] = i;
    pthread_create(&tid[i], NULL, core, &id[i]);
  }

  // we should use a shorter sleeping function like nanosleep()
  sleep(1);
  // main is simulating the data movement by the NIs through the NoC
  for (int i=0; i<10; ++i) {
    // This is VERY incomplete and does not reflect the actual indexing
    // And that it is more than one word in the memory blocks
    allrxmem[0][0] = alltxmem[1][3];
    allrxmem[1][0] = alltxmem[3][0];
    allrxmem[2][0] = alltxmem[2][0];
    allrxmem[3][0] = alltxmem[1][0];
    sleep(1);
    // or maybe better ues yield() to give 
  }

  sleep(1);


  for (int i=0; i<4; ++i) {
    pthread_join(&tid[i], NULL);
  }

printf("\n");
  for (int i=0; i<4; ++i) {
printf("%d\n", alltxmem[i][0]);
}
  for (int i=0; i<4; ++i) {
printf("%d\n", allrxmem[i][0]);
}


  printf("End of simulation\n");
  return 0;
}

