SOURCE = chunk.c common.c debug.c errorHandling.c lexer.c main.c memory.c parser.c
TARGET = lexer

ARGUMENT = test.ardi

.PHONY: all, clean, run

all: ${TARGET}.exe

${TARGET}.exe: ${SOURCE}
	gcc ${SOURCE} -o ${TARGET}.exe

clean:
	del ${TARGET}.exe

run: ${TARGET}.exe
	./${TARGET}.exe ${ARGUMENT}