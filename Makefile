# Compilador y flags
CC=gcc
CFLAGS=-Wall -Iinclude

# Archivos fuente
SRCS=src/cpu.c src/memoria.c src/disco.c src/dma.c src/loader.c src/logger.c src/main.c

# Nombre del ejecutable (en el directorio raiz)
TARGET=maquina_virtual

# Regla principal: compila todos los fuentes directamente al ejecutable
all: $(TARGET)

# Compila todos los archivos fuente en un solo paso
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) -lpthread

# Limpia el ejecutable
clean:
	rm -f $(TARGET)
