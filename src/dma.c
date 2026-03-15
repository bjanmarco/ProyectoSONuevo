// Controlador Direct Memory Access (DMA) simulado
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "../include/dma.h"
#include "../include/disco.h"
#include "../include/memoria.h"
#include "../include/logger.h"

ControladorDma dma;
int interrupcionPendienteDma = 0;

static void finalizarConError() {
    dma.estado = 1;
    dma.ocupado = 0;
    interrupcionPendienteDma = 1;
}

// Hilo asincrono de transferencia
void *hiloTransferenciaDma(void *arg) {
    char bufferSector[TAMANO_SECTOR];
    int resultado, i, valorEntero;
    Palabra palabraTemp;
    (void)arg;

    logDma("Hilo iniciado - Pista=%d, Cilindro=%d, Sector=%d, Dir=%d, Mem=%d",
           dma.pistaSeleccionada, dma.cilindroSeleccionado, dma.sectorSeleccionado,
           dma.direccionIo, dma.direccionMemoria);
    
    usleep(100000);  // 100ms latencia simulada
    
    if (dma.direccionIo == 0) {
        // LECTURA: Disco -> Memoria
        logDma("Operacion: LECTURA (Disco -> Memoria)");
        resultado = leerSectorDisco(dma.pistaSeleccionada, dma.cilindroSeleccionado,
                                    dma.sectorSeleccionado, bufferSector);
        if (resultado != 0) {
            logDma("ERROR: Fallo al leer del disco");
            finalizarConError();
            return NULL;
        }
        
        // convertir caracteres a entero
        valorEntero = 0;
        for (i = 0; i < TAMANO_SECTOR && bufferSector[i] != '\0'; i++) {
            if (bufferSector[i] >= '0' && bufferSector[i] <= '9')
                valorEntero = valorEntero * 10 + (bufferSector[i] - '0');
        }
        palabraTemp = enteroAPalabra(valorEntero);
        escribirMemoria(dma.direccionMemoria, palabraTemp);
        logDma("Datos transferidos a memoria[%d]: %d", dma.direccionMemoria, valorEntero);
        dma.estado = 0;
        
    } else {
        // ESCRITURA: Memoria -> Disco
        logDma("Operacion: ESCRITURA (Memoria -> Disco)");
        palabraTemp = leerMemoria(dma.direccionMemoria);
        valorEntero = palabraAEntero(palabraTemp);
        logDma("Dato leido de memoria[%d]: %d", dma.direccionMemoria, valorEntero);
        
        // convertir entero a string con signo
        bufferSector[0] = (valorEntero < 0) ? '-' : '+';
        if (valorEntero < 0) valorEntero = -valorEntero;
        for (i = TAMANO_SECTOR - 1; i >= 1; i--) {
            bufferSector[i] = '0' + (valorEntero % 10);
            valorEntero /= 10;
        }
        
        resultado = escribirSectorDisco(dma.pistaSeleccionada, dma.cilindroSeleccionado,
                                        dma.sectorSeleccionado, bufferSector);
        if (resultado != 0) {
            logDma("ERROR: Fallo al escribir en disco");
            finalizarConError();
            return NULL;
        }
        dma.estado = 0;
    }
    
    dma.ocupado = 0;
    interrupcionPendienteDma = 1;
    logDma("Transferencia completada - Estado: %s", dma.estado == 0 ? "EXITO" : "ERROR");
    logDma("Interrupcion INT_IO_DONE generada");
    return NULL;
}

// Inicializa registros
void inicializarDma() {
    dma.pistaSeleccionada = 0;
    dma.cilindroSeleccionado = 0;
    dma.sectorSeleccionado = 0;
    dma.direccionIo = 0;
    dma.direccionMemoria = 0;
    dma.estado = 0;
    dma.ocupado = 0;
    interrupcionPendienteDma = 0;
    logDma("Controlador DMA inicializado");
}

void iniciarTransferenciaDma() {
    if (dma.ocupado) {
        logDma("ERROR: DMA ocupado, no se puede iniciar transferencia");
        return;
    }
    dma.ocupado = 1;
    logDma("Iniciando transferencia en hilo separado...");
    
    if (pthread_create(&dma.hiloId, NULL, hiloTransferenciaDma, NULL) != 0) {
        logDma("ERROR: No se pudo crear el hilo de transferencia");
        finalizarConError();
        return;
    }
    pthread_detach(dma.hiloId); 
}

int verificarInterrupcionDma() {
    return interrupcionPendienteDma;
}

