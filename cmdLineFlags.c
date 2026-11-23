#ifndef ARDUINT_CMDLINEFLAGS_C
#define ARDUINT_CMDLINEFLAGS_C

#include <stdio.h>

#include "cmdLineFlags.h"

CmdFlags cmdFlags;

void initCmdFlags() {
  cmdFlags.debugParser = false;
  cmdFlags.debugScanner = false;
  cmdFlags.debugVM = false;
  cmdFlags.mode = MODE_UNSET;
  cmdFlags.path = "";
  cmdFlags.comPort[0] = '\0'; // Initialise to a length of 0
}

#endif