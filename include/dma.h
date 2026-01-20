#ifndef DMA_H
#define DMA_H

#include <pthread.h>
#include "hardware.h"

// VARIABLES GLOBALES DEL DMA
// Controlador DMA
extern ControladorDma dma;

// Bandera para indicar que el DMA termino y hay interrupcion pendiente
extern int interrupcionPendienteDma;

// PROTOTIPOS DE FUNCIONES DEL DMA

// Cabecera de la funcion que inicia el DMA
void inicializarDma();

// Cabecera de la funcion que inicia una transferencia de E/S en un hilo separado.
void iniciarTransferenciaDma();

// Cabecera de la funcion que verifica si el DMA tiene una interrupcion pendiente.
int verificarInterrupcionDma();

#endif