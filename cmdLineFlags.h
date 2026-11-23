#ifndef CMDLINEFLAGS_H
#define CMDLINEFLAGS_H

#include <stdbool.h>

typedef enum { MODE_UNSET, MODE_RUN, MODE_LEX } ProgramMode;

typedef struct {
  bool debugScanner;
  bool debugParser;
  bool debugVM;
  char comPort[10]; // ex: \\.\COM4 or "\\\\.\\COM4"
  ProgramMode mode;
  const char *path;
} CmdFlags;

extern CmdFlags cmdFlags;

void initCmdFlags();

#endif