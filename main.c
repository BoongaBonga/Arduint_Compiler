#include <stdio.h>
#include <stdlib.h>

#include "cmdLineFlags.h"
#include "collection.h"
#include "debug.h"
#include "errorHandling.h"
#include "object.h"
#include "parser.h"
#include "scanner.h"
#include "vm.h"

static char *readFile(const char *path) {
  FILE *filePtr = fopen(path, "rb");

  if (filePtr == NULL) {
    defaultError("Could not open file \"%s\".", path);
    exit(74);
  }

  // Move ptr to end of file
  fseek(filePtr, 0L, SEEK_END);
  // Get it's current offset
  long int len = ftell(filePtr);
  // Rewind to start of file pos
  rewind(filePtr);

  // Allocate an array to store this file temporarily
  char *sourceBuf = (char *)malloc(len + 1);

  if (sourceBuf == NULL) {
    defaultError("Not enough memory to read file.");
    exit(74);
  }

  size_t bytesRead = fread(sourceBuf, sizeof(char), len, filePtr);
  if (bytesRead < len || ferror(filePtr)) {
    defaultError("Failed to read file.");
    exit(74);
  }

  sourceBuf[bytesRead] = '\0';
  fclose(filePtr);

  return sourceBuf;
}

static void setCmdFlags(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      // Set path
      cmdFlags.path = argv[i];
      continue;
    }

    // Determine flag
    switch (argv[i][1]) {
    case 'd': {
      switch (argv[i][2]) {
      case 's': cmdFlags.debugScanner = true; continue;
      case 'p': cmdFlags.debugParser = true; continue;
      case 'v': cmdFlags.debugVM = true; continue;
      default:  break;
      }
      continue;
    }
    case 'm': {
      if (cmdFlags.mode != MODE_UNSET) {
        defaultError("Cannot set two modes at once.");
        exit(2);
      }
      switch (argv[i][2]) {
      case 'l': cmdFlags.mode = MODE_LEX; continue;
      case 'r': cmdFlags.mode = MODE_RUN; continue;
      default:  break;
      }
      continue;
    }
    case 'p': {
      // Get comPort
      if (i == argc - 1) {
        fprintf(stderr, "Expect COM port name after '-p' flag.");
        exit(2);
      }
      snprintf(cmdFlags.comPort, sizeof(cmdFlags.comPort), "\\\\.\\%s",
               argv[++i]);
      continue;
    }
    default: {
      // Unknown flag
      defaultError("Unknown flag '%s'", argv[i]);
      exit(2);
    }
    }
  }

  if (cmdFlags.mode == MODE_UNSET) {
    defaultError(
        "Please enter a program mode. Either '-mr' (run) or '-ml' (lex).");
    exit(2);
  }
}

int main(int argc, char *argv[]) {
  // Expect at least one argument, source.ardi and at most source, -ds and -dp
  if (argc < 2) {
    defaultError("Too little arguments. Please include [source].ardi and the "
                 "compiler mode.");
    return -1;
  }

  // Set flags
  initCmdFlags();
  setCmdFlags(argc, argv);

  // Then, we open the file and then start lexing
  char *source = readFile(cmdFlags.path);

  // Start lexing the file
  Scanner scanner = initScanner(source);
  scanTokens(&scanner);

  // Optionally print tokens
  if (cmdFlags.debugScanner) printTokens(&scanner.tokens);

  // Parse the tokens & optionally print them
  Parser parser = initParser(&scanner.tokens);
  ObjFunction *program = parse(&parser);

  // Try running program
  if (cmdFlags.mode == MODE_RUN && parser.isValidProgram && !scanner.hadError) {
    VM vm;
    initVM(&parser, &vm);

    interpret(&vm, program);

    printf("Exiting...");
    freeVM(&vm);
  }

  // Close down
  free(source);
  freeScanner(&scanner);
  freeParser(&parser);
  return 0;
}