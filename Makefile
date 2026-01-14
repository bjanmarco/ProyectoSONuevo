# ==============================================================================
# MAKEFILE - Maquina Virtual (Sistema Operativo)
# ==============================================================================
# Estructura del proyecto:
#   include/ - Archivos de cabecera (.h)
#   src/     - Archivos de implementacion (.c)
#   obj/     - Archivos objeto (.o)
#   bin/     - Ejecutable final
# ==============================================================================

# Compilador y banderas
CC = gcc
CFLAGS = -Wall -Wextra -g -I$(INCLUDE_DIR)
LDFLAGS = -lpthread

# Directorios
INCLUDE_DIR = include
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Nombre del ejecutable
TARGET = $(BIN_DIR)/maquina_virtual

# ==============================================================================
# ARCHIVOS FUENTE
# Lista de todos los .c que forman parte del proyecto
# NOTA: Agregar aqui nuevos archivos conforme se desarrollen
# ==============================================================================
SOURCES = $(SRC_DIR)/cpu.c \
          $(SRC_DIR)/memoria.c \
          $(SRC_DIR)/disco.c \
          $(SRC_DIR)/dma.c \
          $(SRC_DIR)/loader.c \
          $(SRC_DIR)/logger.c \
          $(SRC_DIR)/main.c
# Archivos pendientes por desarrollar (descomentar cuando existan):
#         $(SRC_DIR)/consola.c

# Generar lista de objetos a partir de fuentes
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))

# ==============================================================================
# REGLAS DE COMPILACION
# ==============================================================================

# Regla principal: compilar todo
all: dirs $(OBJECTS)
	@echo "====================================="
	@echo " Compilacion completada exitosamente"
	@echo " Objetos generados en: $(OBJ_DIR)/"
	@echo "====================================="
	@echo ""
	@echo " NOTA: Falta main.c para generar ejecutable"
	@echo " Cuando exista main.c, ejecutar: make link"

# Crear directorios necesarios
dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Regla para compilar .c a .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "[CC] Compilando $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Regla para enlazar (cuando exista main.c)
link: dirs $(OBJECTS)
	@echo "[LD] Enlazando ejecutable..."
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Ejecutable generado: $(TARGET)"

# ==============================================================================
# LIMPIEZA
# ==============================================================================

# Limpiar objetos
clean:
	@echo "Limpiando archivos objeto..."
	rm -rf $(OBJ_DIR)/*.o

# Limpiar todo
cleanall: clean
	@echo "Limpiando ejecutable..."
	rm -rf $(BIN_DIR)/*

# ==============================================================================
# AYUDA
# ==============================================================================

help:
	@echo "Makefile - Maquina Virtual"
	@echo ""
	@echo "Comandos disponibles:"
	@echo "  make        - Compila todos los .c a .o"
	@echo "  make link   - Compila y enlaza el ejecutable"
	@echo "  make clean  - Elimina archivos .o"
	@echo "  make cleanall - Elimina .o y ejecutable"
	@echo "  make help   - Muestra esta ayuda"
	@echo ""
	@echo "Estructura:"
	@echo "  include/  - Archivos .h"
	@echo "  src/      - Archivos .c"
	@echo "  obj/      - Archivos .o (generados)"
	@echo "  bin/      - Ejecutable (generado)"

# Marcar reglas que no son archivos
.PHONY: all dirs link clean cleanall help
