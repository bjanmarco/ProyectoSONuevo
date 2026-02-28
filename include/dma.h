#ifndef DMA_H
#define DMA_H

#include <pthread.h>
#include "hardware.h"

// variables globales

// estructura de control del DMA (registros de que copiar a donde).
extern ControladorDma dma;

// bandera que se levanta cuando el DMA termina.
// el CPU la revisa en cada ciclo para ver si tiene que atender la interrupcion.
extern int interrupcionPendienteDma;

// funciones del DMA

// pone todo en cero.
void inicializarDma();

// crea un hilo (thread) nuevo de verdad.
// eso permite que la copia de datos ocurra al mismo tiempo que el CPU ejecuta instrucciones.
// sin esto, la maquina se congelaria cada vez que leemos del disco.
void iniciarTransferenciaDma();

// el CPU llama a esto para preguntar si termino el DMA
int verificarInterrupcionDma();

#endif
