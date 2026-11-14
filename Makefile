SOURCE = chunk.c cmdLineFlags.c collection.c common.c debug.c errorHandling.c scanner.c main.c memory.c object.c parser.c vm.c
TARGET = arduint
FLAGS = -ds -dp -dv

INPUTFILE = test.ardi

.PHONY: all, clean, run, lex, debug_lex, debug_run

all: ${TARGET}.exe

${TARGET}.exe: ${SOURCE}
	gcc ${SOURCE} -o ${TARGET}.exe

clean:
	del ${TARGET}.exe

run: ${TARGET}.exe
	./${TARGET}.exe ${INPUTFILE} -mr 

lex: ${TARGET}.exe
	./${TARGET}.exe ${INPUTFILE} -ml

debug_lex: ${TARGET}.exe
	./${TARGET}.exe ${INPUTFILE} -ml -ds -dp 

debug_run: ${TARGET}.exe
	./${TARGET}.exe ${INPUTFILE} -mr -ds -dp -dv
