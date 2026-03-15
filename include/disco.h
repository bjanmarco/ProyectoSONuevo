#ifndef DISCO_H
#define DISCO_H

#include "hardware.h"

extern DiscoDuro discoDuro;

void inicializarDisco();
int leerSectorDisco(int pista, int cilindro, int sector, char *buffer);
int escribirSectorDisco(int pista, int cilindro, int sector, const char *buffer);

#endif
