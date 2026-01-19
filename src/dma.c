#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "../include/dma.h"
#include "../include/disco.h"
#include "../include/memoria.h"

// Controlador DMA
ControladorDma dma;

// Bandera de interrupcion pendiente (1 = pendiente, 0 = no)
int interrupcionPendienteDma = 0;

// Funcion auxiliar para finalizar transferencia con error
static void finalizarConError() {
    dma.estado = 1;
    dma.ocupado = 0;
    interrupcionPendienteDma = 1;
}

void *hiloTransferenciaDma(void *arg) {
    char bufferSector[TAMANO_SECTOR];
    int resultado, i, valorEntero;
    Palabra palabraTemp;
    (void)arg;
    
    printf("[DMA] Hilo iniciado - Pista=%d, Cilindro=%d, Sector=%d, Dir=%d, Mem=%d\n",
           dma.pistaSeleccionada, dma.cilindroSeleccionado, dma.sectorSeleccionado,
           dma.direccionIo, dma.direccionMemoria);
    
    usleep(100000);  // 100ms latencia simulada
    
    if (dma.direccionIo == 0) {
        // LECTURA: Disco -> Memoria
        printf("[DMA] Operacion: LECTURA (Disco -> Memoria)\n");
        resultado = leerSectorDisco(dma.pistaSeleccionada, dma.cilindroSeleccionado,
                                    dma.sectorSeleccionado, bufferSector);
        if (resultado != 0) {
            printf("[DMA] ERROR: Fallo al leer del disco\n");
            finalizarConError();
            return NULL;
        }
        
        // Convertir caracteres a entero
        valorEntero = 0;
        for (i = 0; i < TAMANO_SECTOR && bufferSector[i] != '\0'; i++) {
            if (bufferSector[i] >= '0' && bufferSector[i] <= '9')
                valorEntero = valorEntero * 10 + (bufferSector[i] - '0');
        }
        palabraTemp = enteroAPalabra(valorEntero);
        escribirMemoria(dma.direccionMemoria, palabraTemp);
        printf("[DMA] Datos transferidos a memoria[%d]: %d\n", dma.direccionMemoria, valorEntero);
        dma.estado = 0;
        
    } else {
        // ESCRITURA: Memoria -> Disco
        printf("[DMA] Operacion: ESCRITURA (Memoria -> Disco)\n");
        palabraTemp = leerMemoria(dma.direccionMemoria);
        valorEntero = palabraAEntero(palabraTemp);
        printf("[DMA] Dato leido de memoria[%d]: %d\n", dma.direccionMemoria, valorEntero);
        
        // Convertir entero a string con signo
        bufferSector[0] = (valorEntero < 0) ? '-' : '+';
        if (valorEntero < 0) valorEntero = -valorEntero;
        for (i = TAMANO_SECTOR - 1; i >= 1; i--) {
            bufferSector[i] = '0' + (valorEntero % 10);
            valorEntero /= 10;
        }
        
        resultado = escribirSectorDisco(dma.pistaSeleccionada, dma.cilindroSeleccionado,
                                        dma.sectorSeleccionado, bufferSector);
        if (resultado != 0) {
            printf("[DMA] ERROR: Fallo al escribir en disco\n");
            finalizarConError();
            return NULL;
        }
        dma.estado = 0;
    }
    
    dma.ocupado = 0;
    interrupcionPendienteDma = 1;
    printf("[DMA] Transferencia completada - Estado: %s\n", dma.estado == 0 ? "EXITO" : "ERROR");
    printf("[DMA] Interrupcion INT_IO_DONE generada\n");
    return NULL;
}

void inicializarDma() {
    dma.pistaSeleccionada = 0;
    dma.cilindroSeleccionado = 0;
    dma.sectorSeleccionado = 0;
    dma.direccionIo = 0;
    dma.direccionMemoria = 0;
    dma.estado = 0;
    dma.ocupado = 0;
    interrupcionPendienteDma = 0;
    printf("[DMA] Controlador DMA inicializado\n");
}

void iniciarTransferenciaDma() {
    if (dma.ocupado) {
        printf("[DMA] ERROR: DMA ocupado, no se puede iniciar transferencia\n");
        return;
    }
    dma.ocupado = 1;
    printf("[DMA] Iniciando transferencia en hilo separado...\n");
    
    if (pthread_create(&dma.hiloId, NULL, hiloTransferenciaDma, NULL) != 0) {
        printf("[DMA] ERROR: No se pudo crear el hilo de transferencia\n");
        finalizarConError();
        return;
    }
    pthread_detach(dma.hiloId);
}

int verificarInterrupcionDma() {
    return interrupcionPendienteDma;
}

