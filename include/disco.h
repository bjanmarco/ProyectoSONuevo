#ifndef DISCO_H
#define DISCO_H

#include "hardware.h"

// este es el arreglo de 3 niveles
extern DiscoDuro discoDuro;

// PROTOTIPOS DE FUNCIONES DEL DISCO

// Cabecera de la funcion que inicia el disco
void inicializarDisco();

// Cabecera de la funcion que lee el disco
int leerSectorDisco(int pista, int cilindro, int sector, char *buffer);

// Cabecera de la funcion que escribe en el disco
int escribirSectorDisco(int pista, int cilindro, int sector, const char *buffer);

#endif
