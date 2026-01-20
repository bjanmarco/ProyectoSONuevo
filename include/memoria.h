#ifndef MEMORIA_H
#define MEMORIA_H

#include <semaphore.h>
#include "hardware.h"

// VARIABLES GLOBALES DE MEMORIA
// Declaradas como extern para ser definidas en memoria.c

// Arreglo principal de memoria que son 2000 palabras de 8 digitos
extern Palabra memoriaPrincipal[TAMANO_MEMORIA];

// Semaforo para controlar DMA y disco
extern sem_t bloqueoBus;

// PROTOTIPOS DE FUNCIONES DE MEMORIA

// Cabecera de la funcion para inicializar la memoria
void inicializarMemoria();

// Cabecera de la funcion para finalizar la memoria
void finalizarMemoria();

// Cabecera de la funcion para leer la memoria
Palabra leerMemoria(int direccion);

// Cabecera de la funcion para escribir en memoria
void escribirMemoria(int direccion, Palabra dato);

#endif 
