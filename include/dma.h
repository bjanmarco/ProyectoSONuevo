#ifndef DMA_H
#define DMA_H

#include <pthread.h>
#include "hardware.h"

extern ControladorDma dma;
extern int interrupcionPendienteDma;

void inicializarDma();
void iniciarTransferenciaDma();
int verificarInterrupcionDma();

#endif
