CC=gcc
CFLAGS=-Wall -Iinclude

OBJS=obj/cpu.o obj/memoria.o obj/disco.o obj/dma.o obj/loader.o obj/logger.o obj/main.o

all: carpetas bin/maquina_virtual limpiar_objetos

carpetas:
	mkdir -p obj bin

bin/maquina_virtual: $(OBJS)
	$(CC) $(OBJS) -o bin/maquina_virtual -lpthread

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

limpiar_objetos:
	rm -f obj/*.o

clean:
	rm -f bin/maquina_virtual obj/*.o
