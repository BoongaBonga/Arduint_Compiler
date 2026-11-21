SRC = main.c serial.c
TARGET = output

.PHONY: all, clean, run

all: ${TARGET}.exe

${TARGET}.exe: ${SRC}
	gcc ${SRC} -o ${TARGET}.exe

clean:
	del ${TARGET}.exe

run: ${TARGET}.exe
	./${TARGET}.exe