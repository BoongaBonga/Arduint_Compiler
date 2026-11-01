#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "errorHandling.h"
#include "lexer.h"
#include "parser.h"

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

int main(int argc, char *argv[]) {
  // Expect one argument, source.ardi
  if (argc < 2) {
    defaultError("Too little arguments. Please include [source].ardi.");
    return -1;
  } else if (argc > 2) {
    defaultError("Too many arguments. Please enter only [source].ardi.");
    return -1;
  }

  // Then, we open the file and then start lexing
  char *source = readFile(argv[1]);

  // Start lexing the file
  Scanner lexer = initScanner(source);
  scanTokens(&lexer);

  printTokens(&lexer.tokens);

  Parser parser = initParser(&lexer.tokens);

  parse(&parser);

  printChunk(&parser.chunk, "Program");

  // Close down
  free(source);
  freeScanner(&lexer);
  freeParser(&parser);
}