#ifndef LOADER_H
#define LOADER_H

#include "hardware.h"

// constantes 
#define MAX_NOMBRE_PROGRAMA 50 // tamanio max de un programa
#define MAX_LINEA 100 // tamanio max de una linea

// struct para recordar que cargamos
// necesitamos saber donde empieza (_start), cuantas lineas son,
// y donde lo pusimos (RB/RL) para configurar el CPU antes de correr.
typedef struct {
    char nombre[MAX_NOMBRE_PROGRAMA];   
    int lineaInicio;                    // aqui es donde salta el PC al empezar
    int numeroPalabras;                 // tamanio total del codigo
    int direccionBase;                  // RB (donde comienza en memoria fisica)
    int direccionLimite;                // RL (donde termina)
} InfoPrograma;

// struct para recordar archivos cargados en el disco duro (FAT simulada)
typedef struct {
    char nombre[MAX_NOMBRE_PROGRAMA];
    int lineaInicio;                    // _start
    int numeroPalabras;                 // numero de instrucciones
    int cilindroInicio;
    int pistaInicio;
    int sectorInicio;
    int ocupado;                        // 1 si tiene un programa, 0 si esta libre
} DirectorioPrograma;


// variables globales
// con esta variable recuerda cual es la proxima celda libre de RAM
// para que si cargamos varios programas no se pisen entre ellos.
extern int siguienteDireccionDisponible;

// Indice global de la posicion en disco para grabar el siguiente archivo
extern int siguienteCilindroDisponible;
extern int siguientePistaDisponible;
extern int siguienteSectorDisponible;

// Arreglo FAT
#define MAX_PROGRAMAS_DISCO 20
extern DirectorioPrograma directorioDisco[MAX_PROGRAMAS_DISCO];

extern InfoPrograma programaActual;

// funciones del Loader

// reinicia el puntero de "proxima direccion libre" (normalmente a 300).
void inicializarLoader();

// Funciones divididas de Loader (Requisito Arquitectura Archivo -> Disco -> RAM)
// 1. Lee el .txt, lo valida estrictamente y lo escribe en el disco duro
int cargarProgramaEnDisco(const char *rutaArchivo);

// 2. Busca el programa en el disco duro y lo vierte a la RAM asignada, creando su PCB
int cargarProgramaEnMemoria(const char *nombrePrograma);

// (Deprecada, mantenida por compatibilidad temporal)
int cargarPrograma(const char *rutaArchivo, int direccionDestino);

// configura los registros del CPU (PC, RB, RL, SP) usando la info
// del ultimo programa que cargamos deja todo listo para ejecutar
void prepararEjecucion();

#endif 
