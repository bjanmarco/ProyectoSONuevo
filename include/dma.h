// ARCHIVO: dma.h
// DMA = Direct Memory Access.
// Este modulo es el "ayudante" del CPU. Se encarga de mover datos entre Disco y RAM
// en paralelo, para que el CPU pueda seguir haciendo otras cosas.

#ifndef DMA_H
#define DMA_H

#include <pthread.h>
#include "hardware.h"

// --- Variables Globales ---

// Estructura de control del DMA (registros de que copiar a donde).
extern ControladorDma dma;

// Bandera que se levanta cuando el DMA termina.
// El CPU la revisa en cada ciclo para ver si tiene que atender la interrupcion.
extern int interrupcionPendienteDma;

// --- Funciones del DMA ---

// Pone todo en cero.
void inicializarDma();

// Esta es la magia. Crea un hilo (thread) nuevo de verdad.
// Eso permite que la copia de datos ocurra AL MISMO TIEMPO que el CPU ejecuta instrucciones.
// Sin esto, la maquina se congelaria cada vez que leemos del disco.
void iniciarTransferenciaDma();

// El CPU llama a esto para preguntar: "¿Ya termino el DMA?"
int verificarInterrupcionDma();

#endif
