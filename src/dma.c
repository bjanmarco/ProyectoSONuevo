// este modulo es como un ayudante del CPU para escribir y leer datos entre memoria y disco
// en paralelo para que el CPU pueda seguir haciendo otras cosas
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "../include/dma.h"
#include "../include/disco.h"
#include "../include/memoria.h"

// es la estructura de control del DMA (registros de que copiar a donde).
ControladorDma dma;

// bandera de interrupcion pendiente (1 = pendiente, 0 = no)
int interrupcionPendienteDma = 0;
static int hiloActivo = 0;

typedef struct {
    int pistaSeleccionada;
    int cilindroSeleccionado;
    int sectorSeleccionado;
    int direccionIo;
    int direccionMemoria;
} SolicitudDma;

// funcion auxiliar para finalizar transferencia con error
static void finalizarConError() {
    dma.estado = 1; // marca error
    dma.ocupado = 0; // libera
    interrupcionPendienteDma = 1;
}

// es la funcion principal que corre en un hilo para no congelar la maquina
void *hiloTransferenciaDma(void *arg) {
    SolicitudDma *solicitud = (SolicitudDma *)arg;
    char bufferSector[TAMANO_SECTOR];
    int resultado, i, valorEntero;
    Palabra palabraTemp;

    int pista = solicitud->pistaSeleccionada;
    int cilindro = solicitud->cilindroSeleccionado;
    int sector = solicitud->sectorSeleccionado;
    int direccionIo = solicitud->direccionIo;
    int direccionMemoria = solicitud->direccionMemoria;

    free(solicitud);

    // muestra los detalles
    printf("[DMA] Hilo iniciado - Pista=%d, Cilindro=%d, Sector=%d, Dir=%d, Mem=%d\n",
           pista, cilindro, sector, direccionIo, direccionMemoria);
    
    usleep(100000);  // 100ms latencia simulada
    
    if (direccionIo == 0) {
        // LECTURA: Disco -> Memoria
        printf("[DMA] Operacion: LECTURA (Disco -> Memoria)\n");
        resultado = leerSectorDisco(pista, cilindro, sector, bufferSector);
        if (resultado != 0) {
            printf("[DMA] ERROR: Fallo al leer del disco\n");
            finalizarConError();
            return NULL;
        }
        
        // convertir caracteres a entero
        valorEntero = 0;
        for (i = 0; i < TAMANO_SECTOR && bufferSector[i] != '\0'; i++) {
            if (bufferSector[i] >= '0' && bufferSector[i] <= '9')
                valorEntero = valorEntero * 10 + (bufferSector[i] - '0');
        } // recorre el buffer y convierte los caracteres a entero
        palabraTemp = enteroAPalabra(valorEntero);
        escribirMemoria(direccionMemoria, palabraTemp);
        printf("[DMA] Datos transferidos a memoria[%d]: %d\n", direccionMemoria, valorEntero);
        dma.estado = 0;
        
    } else {
        // ESCRITURA: Memoria -> Disco
        printf("[DMA] Operacion: ESCRITURA (Memoria -> Disco)\n");
        palabraTemp = leerMemoria(direccionMemoria);
        valorEntero = palabraAEntero(palabraTemp);
        printf("[DMA] Dato leido de memoria[%d]: %d\n", direccionMemoria, valorEntero);
        
        // convertir entero a string con signo
        bufferSector[0] = (valorEntero < 0) ? '-' : '+';
        if (valorEntero < 0) valorEntero = -valorEntero;
        for (i = TAMANO_SECTOR - 1; i >= 1; i--) {
            bufferSector[i] = '0' + (valorEntero % 10);
            valorEntero /= 10;
        } // conierte el entero al formato de la maquina (palabra)
        
        resultado = escribirSectorDisco(pista, cilindro, sector, bufferSector);
        if (resultado != 0) {
            printf("[DMA] ERROR: Fallo al escribir en disco\n");
            finalizarConError();
            return NULL;
        }
        dma.estado = 0;
    }
    
    printf("[DMA] Transferencia completada - Estado: %s\n", dma.estado == 0 ? "EXITO" : "ERROR");
    printf("[DMA] Interrupcion INT_IO_DONE generada\n");
    interrupcionPendienteDma = 1; // levanta la bandera de interrupcion
    dma.ocupado = 0;
    return NULL;
}

// pone todo en cero listo para usar
void inicializarDma() {
    dma.pistaSeleccionada = 0;
    dma.cilindroSeleccionado = 0;
    dma.sectorSeleccionado = 0;
    dma.direccionIo = 0;
    dma.direccionMemoria = 0;
    dma.estado = 0;
    dma.ocupado = 0;
    interrupcionPendienteDma = 0;
    hiloActivo = 0;
    printf("[DMA] Controlador DMA inicializado\n");
}

int esperarDmaLibre(int timeoutMs) {
    int esperado = 0;
    while (dma.ocupado) {
        if (timeoutMs >= 0 && esperado >= timeoutMs) {
            return 1;
        }
        usleep(1000);
        esperado++;
    }
    return 0;
}

void iniciarTransferenciaDma() {
    SolicitudDma *solicitud;

    if (esperarDmaLibre(500) != 0) {
        printf("[DMA] ERROR: DMA ocupado demasiado tiempo, solicitud cancelada\n");
        dma.estado = 1;
        interrupcionPendienteDma = 1;
        return;
    }

    if (hiloActivo) {
        pthread_join(dma.hiloId, NULL);
        hiloActivo = 0;
    }

    solicitud = (SolicitudDma *)malloc(sizeof(SolicitudDma));
    if (solicitud == NULL) {
        printf("[DMA] ERROR: No se pudo reservar memoria para la solicitud DMA\n");
        finalizarConError();
        return;
    }

    solicitud->pistaSeleccionada = dma.pistaSeleccionada;
    solicitud->cilindroSeleccionado = dma.cilindroSeleccionado;
    solicitud->sectorSeleccionado = dma.sectorSeleccionado;
    solicitud->direccionIo = dma.direccionIo;
    solicitud->direccionMemoria = dma.direccionMemoria;

    dma.ocupado = 1;
    printf("[DMA] Iniciando transferencia en hilo separado...\n");
    
    // crea un hilo nuevo de verdad
    if (pthread_create(&dma.hiloId, NULL, hiloTransferenciaDma, solicitud) != 0) {
        printf("[DMA] ERROR: No se pudo crear el hilo de transferencia\n");
        free(solicitud);
        finalizarConError();
        return;
    }
    hiloActivo = 1;
}

int verificarInterrupcionDma() { // devuelve el valor de la bandera de interrupcion
    return interrupcionPendienteDma; // para que el CPU sepa si debe prestar atencion al DMA
}

void finalizarDma() {
    if (esperarDmaLibre(2000) != 0) {
        printf("[DMA] ADVERTENCIA: DMA no finalizo a tiempo durante cierre\n");
        return;
    }
    if (hiloActivo) {
        pthread_join(dma.hiloId, NULL);
        hiloActivo = 0;
    }
}

