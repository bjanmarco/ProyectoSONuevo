#Makefile para la compilacion de todo el codigo

CC=gcc
CFLAGS=-Wall -Iinclude

SRCS=src/cpu.c src/memoria.c src/disco.c src/dma.c src/loader.c src/logger.c src/main.c src/procesos.c

TARGET=maquinaVirtual

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) -lpthread

clean:
	rm -f $(TARGET)
