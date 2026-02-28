#ifndef DISCO_H
#define DISCO_H

#include "hardware.h"

// variable global

// el disco entero es un arreglo de 3 niveles
extern DiscoDuro discoDuro;

// funciones del disco

// llena todo de ceros para que no haya basura al empezar.
void inicializarDisco();

// simulan la lectura/escritura fisica. 
// validan que la pista/cilindro/sector existan antes de intentar copiar nada.
// retornan 0 si todo salio bien.
int leerSectorDisco(int pista, int cilindro, int sector, char *buffer);
int escribirSectorDisco(int pista, int cilindro, int sector, const char *buffer);

#endif
