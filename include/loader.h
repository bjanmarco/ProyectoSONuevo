// ARCHIVO: loader.h
// El Portero.
// Este modulo lee los archivos de programa (nuestros "ejecutables" de texto)
// y los carga byte a byte en la memoria RAM para que el CPU los pueda ejecutar.

#ifndef LOADER_H
#define LOADER_H

#include "hardware.h"

// --- Constantes ---
// Nombres de archivos y buffers.
#define MAX_NOMBRE_PROGRAMA 50
#define MAX_LINEA 100

// Struct para recordar que cargamos.
// Necesitamos saber donde empieza (_start), cuantas lineas son,
// y donde lo pusimos (RB/RL) para configurar el CPU antes de correr.
typedef struct {
    char nombre[MAX_NOMBRE_PROGRAMA];   
    int lineaInicio;                    // Aqui es donde salta el PC al empezar
    int numeroPalabras;                 // Tamaño total del codigo
    int direccionBase;                  // RB (Donde comienza en memoria FISICA)
    int direccionLimite;                // RL (Donde termina)
} InfoPrograma;

// --- Variables Globales ---

// Puntero inteligente: recuerda cual es la proxima celda libre de RAM
// para que si cargamos varios programas no se pisen entre ellos.
extern int siguienteDireccionDisponible;

extern InfoPrograma programaActual;

// --- Funciones del Loader ---

// Reinicia el puntero de "proxima direccion libre" (normalmente a 300).
void inicializarLoader();

// La funcion heavy.
// 1. Abre el archivo.
// 2. Lee linea por linea (metadata y codigo).
// 3. Escribe en memoriaPrincipal[].
// 4. Si direccionDestino es -1, decide el solo donde ponerlo (modo automatico).
int cargarPrograma(const char *rutaArchivo, int direccionDestino);

// Configura los registros del CPU (PC, RB, RL, SP) usando la info
// del ultimo programa que cargamos. Deja todo listo para el comando 'run'.
void prepararEjecucion();


#endif 
