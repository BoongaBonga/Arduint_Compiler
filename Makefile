SOURCE = chunk.c cmdLineFlags.c collection.c common.c debug.c errorHandling.c scanner.c main.c memory.c object.c parser.c vm.c
TARGET = arduint

INPUTFILE = test.ardi

.PHONY: all, clean, run, debug

all: ${TARGET}.exe

${TARGET}.exe: ${SOURCE}
	gcc ${SOURCE} -o ${TARGET}.exe

clean:
	del ${TARGET}.exe

run: ${TARGET}.exe
	./${TARGET}.exe ${INPUTFILE}

debug: ${TARGET}.exe
	./${TARGET}.exe ${INPUTFILE} -ds -dp