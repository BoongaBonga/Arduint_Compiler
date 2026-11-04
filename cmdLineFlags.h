#ifndef CMDLINEFLAGS_H
#define CMDLINEFLAGS_H

#include <stdbool.h>

typedef struct {
  bool debugScanner;
  bool debugParser;
  const char *path;
} CmdFlags;

extern CmdFlags cmdFlags;

void initCmdFlags();

#endif