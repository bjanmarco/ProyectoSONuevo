#Makefile para la compilacion de todo el codigo

CC=gcc
CFLAGS=-Wall -Iinclude

SRCS=src/cpu.c src/memoria.c src/disco.c src/dma.c src/loader.c src/logger.c src/proceso.c src/main.c

TARGET=maquinaVirtual

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) -lpthread

clean:
	rm -f $(TARGET)
