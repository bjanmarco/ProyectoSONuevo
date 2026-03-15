#ifndef LOADER_H
#define LOADER_H

#include "hardware.h"

#define MAX_NOMBRE_PROGRAMA 50
#define MAX_LINEA 512

typedef struct {
    char nombre[MAX_NOMBRE_PROGRAMA];   
    int lineaInicio;
    int numeroPalabras;
    int direccionBase;
    int direccionLimite;
} InfoPrograma;

typedef struct {
    char nombre[MAX_NOMBRE_PROGRAMA];
    int lineaInicio;
    int numeroPalabras;
    int cilindroInicio;
    int pistaInicio;
    int sectorInicio;
    int ocupado;
} DirectorioPrograma;


extern int siguienteCilindroDisponible;
extern int siguientePistaDisponible;
extern int siguienteSectorDisponible;

#define MAX_PROGRAMAS_DISCO 20
extern DirectorioPrograma directorioDisco[MAX_PROGRAMAS_DISCO];

extern InfoPrograma programaActual;

void inicializarLoader();
int cargarProgramaEnDisco(const char *rutaArchivo);
int cargarProgramaEnMemoria(const char *nombrePrograma);
int cargarPrograma(const char *rutaArchivo, int direccionDestino);
void prepararEjecucion();

#endif 
