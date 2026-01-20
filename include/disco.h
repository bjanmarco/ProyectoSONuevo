// ARCHIVO: disco.h
// Simulacion de un disco duro. No guardamos en un archivo real binario,
// si no que simulamos la estructura geometrica (Plato/Pista/Sector) en memoria.

#ifndef DISCO_H
#define DISCO_H

#include "hardware.h"

// --- Variables Globales ---

// El disco entero. Es un array 3D gigante.
extern DiscoDuro discoDuro;

// --- Funciones del Disco ---

// Llena todo de ceros para que no haya basura al empezar.
void inicializarDisco();

// Simulan la lectura/escritura fisica. 
// Validan que la Pista/Cilindro/Sector existan antes de intentar copiar nada.
// Retornan 0 si todo salio bien.
int leerSectorDisco(int pista, int cilindro, int sector, char *buffer);
int escribirSectorDisco(int pista, int cilindro, int sector, const char *buffer);

#endif
