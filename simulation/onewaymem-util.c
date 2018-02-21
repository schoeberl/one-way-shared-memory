/*
  A software simulation of the One-Way Shared Memory
  Run on Patmos (the target) (and the PC)

  Copyright: CBS, DTU
  Authors: Rasmus Ulslev Pedersen, Martin Schoeberl
  License: Simplified BSD
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#ifndef RUNONPATMOS
#include <time.h>
#endif
//#include <machine/patmos.h>
#include "onewaysim.h"

