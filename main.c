#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (argv[i][0] == '-') {
      // Detect errors
      if (strlen(argv[i]) == 3) {
        // Determine flag
        if (argv[i][1] == 'd') {
          switch (argv[i][2]) {
          case 's': cmdFlags.debugScanner = true; continue;
          case 'p': cmdFlags.debugParser = true; continue;
          case 'v': cmdFlags.debugVM = true; continue;
          default:  break;
          }
        } else if (argv[i][1] == 'm') {
          if (cmdFlags.mode != MODE_UNSET) {
            defaultError("Cannot set two modes at once.");
            exit(2);
          }
          switch (argv[i][2]) {
          case 'l': cmdFlags.mode = MODE_LEX; continue;
          case 'r': cmdFlags.mode = MODE_RUN; continue;
          default:  break;
          }
        }
      }

      // Unknown flag
      defaultError("Unknown flag '%s'", argv[i]);
      exit(2);
    } else {
      // Set path
      cmdFlags.path = argv[i];
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
    defaultError("Too little arguments. Please include [source].ardi.");
    return -1;
  } else if (argc > 6) {
    defaultError(
        "Too many arguments. Please enter only [source].ardi and any flags.");
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

  if (cmdFlags.debugScanner) printTokens(&scanner.tokens);

  Parser parser = initParser(&scanner.tokens);
  parse(&parser);

  if (cmdFlags.debugParser) printChunk(&parser.chunk, "Program");

  if (cmdFlags.mode == MODE_RUN && parser.isValidProgram && !scanner.hadError) {
    VM vm;
    initVM(&parser, &vm);
    interpret(&vm, &parser.chunk);

    freeVM(&vm);
  }

  // Close down
  free(source);
  freeScanner(&scanner);
  freeParser(&parser);
}