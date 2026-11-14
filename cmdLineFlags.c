#ifndef ARDUINT_CMDLINEFLAGS_C
#define ARDUINT_CMDLINEFLAGS_C

#include "cmdLineFlags.h"

CmdFlags cmdFlags;

void initCmdFlags() {
  cmdFlags.debugParser = false;
  cmdFlags.debugScanner = false;
  cmdFlags.debugVM = false;
  cmdFlags.mode = MODE_UNSET;
  cmdFlags.path = "";
}

#endif