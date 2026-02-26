#ifndef LOADER_H
#define LOADER_H

#include "hardware.h"

// constantes 
#define MAX_NOMBRE_PROGRAMA 50 // tamanio max de un programa
#define MAX_LINEA 512 // tamanio max de una linea

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

// variables globales
// con esta variable recuerda cual es la proxima celda libre de RAM
// para que si cargamos varios programas no se pisen entre ellos.
extern int siguienteDireccionDisponible;

extern InfoPrograma programaActual;

// funciones del Loader

// reinicia el puntero de "proxima direccion libre" (normalmente a 300).
void inicializarLoader();

// la funcion central
// abre el archivo.
// lee linea por linea
// escribe en memoriaPrincipal
// si direccionDestino es -1, decide el solo donde ponerlo
int cargarPrograma(const char *rutaArchivo, int direccionDestino);

// configura los registros del CPU (PC, RB, RL, SP) usando la info
// del ultimo programa que cargamos deja todo listo para ejecutar
void prepararEjecucion();

#endif 
