#ifndef LOADER_H
#define LOADER_H

#include "hardware.h"

// CONSTANTES DEL LOADER
// Tamanio maximo del nombre de un programa
#define MAX_NOMBRE_PROGRAMA 50

// Tamanio maximo de una linea del archivo
#define MAX_LINEA 100

// Esta es una estrutura para guardar la info del archivo
typedef struct {
    char nombre[MAX_NOMBRE_PROGRAMA];   // Nombre del programa
    int lineaInicio;                    // Valor de _start (donde empieza ejecucion)
    int numeroPalabras;                 // Cantidad de instrucciones
    int direccionBase;                  // RB asignado (direccion fisica)
    int direccionLimite;                // RL asignado (direccion fisica)
} InfoPrograma;

// VARIABLES GLOBALES DEL LOADER
// Siguiente direccion de memoria disponible para cargar programas
extern int siguienteDireccionDisponible;

// Informacion del ultimo programa cargado
extern InfoPrograma programaActual;

// PROTOTIPOS DE FUNCOONES DEL LOADER

// Cabecera de la funcion para inciar el loader
void inicializarLoader();

// Cabecera de la funcion para cargar un programa
int cargarPrograma(const char *rutaArchivo);

// Cabecera de la funcion para preparar la ejecucion
void prepararEjecucion();

// Cabecera de la funcion para reiniciar el loader
void reiniciarLoader();

#endif 
