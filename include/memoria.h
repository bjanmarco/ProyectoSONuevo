#ifndef MEMORIA_H
#define MEMORIA_H

#include <semaphore.h>
#include "hardware.h"

extern Palabra memoriaPrincipal[TAMANO_MEMORIA];
extern sem_t bloqueoBus;

void inicializarMemoria();
void finalizarMemoria();
Palabra leerMemoria(int direccion);
void escribirMemoria(int direccion, Palabra dato);
void mostrarEstadisticasMemoria();

#endif
