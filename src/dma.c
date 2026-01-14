/*
 * ============================================================================
 * ARCHIVO: dma.c
 * DESCRIPCION: Implementacion del controlador DMA (Acceso Directo a Memoria).
 *              Ejecuta transferencias disco-memoria en un hilo separado.
 *              Usa el semaforo bloqueoBus para arbitraje con el CPU.
 *              Segun especificaciones de prueba.txt seccion 4.
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "../include/dma.h"
#include "../include/disco.h"
#include "../include/memoria.h"

/* ============================================================================
 * DEFINICION DE VARIABLES GLOBALES
 * ============================================================================ */

// Controlador DMA
ControladorDma dma;

// Bandera de interrupcion pendiente del DMA
// Cuando el DMA termina, pone esta bandera a 1 para que el CPU la detecte
int interrupcionPendienteDma = 0;

/* ============================================================================
 * FUNCION DEL HILO DE TRANSFERENCIA
 * ============================================================================ */

/*
 * hiloTransferenciaDma
 * --------------------
 * Funcion que ejecuta el hilo del DMA.
 * Realiza la transferencia entre disco y memoria.
 * 
 * Parametros:
 *   arg - No utilizado (requerido por pthread)
 * 
 * Flujo:
 * 1. Lee los parametros del DMA (pista, cilindro, sector, direccion)
 * 2. Lee del disco (sin usar bus) o lee de memoria (usa bus)
 * 3. Escribe en memoria (usa bus) o escribe en disco (sin usar bus)
 * 4. Establece estado y genera interrupcion
 */
void *hiloTransferenciaDma(void *arg) {
    char bufferSector[TAMANO_SECTOR];  // Buffer temporal para 9 caracteres
    int resultado;
    int i;
    Palabra palabraTemp;
    int valorEntero;
    
    // No usamos el argumento
    (void)arg;
    
    printf("[DMA] Hilo iniciado - Transferencia en curso\n");
    printf("[DMA] Parametros: Pista=%d, Cilindro=%d, Sector=%d, Dir=%d, Mem=%d\n",
           dma.pistaSeleccionada,
           dma.cilindroSeleccionado,
           dma.sectorSeleccionado,
           dma.direccionIo,
           dma.direccionMemoria);
    
    // Simular tiempo de acceso al disco (operacion lenta)
    usleep(100000);  // 100ms de latencia simulada
    
    if (dma.direccionIo == 0) {
        // =========================================================
        // LECTURA: Disco -> Memoria
        // 1. Leer del disco (NO usa bus)
        // 2. Escribir en memoria (SI usa bus)
        // =========================================================
        
        printf("[DMA] Operacion: LECTURA (Disco -> Memoria)\n");
        
        // Paso 1: Leer sector del disco (no necesita bus)
        resultado = leerSectorDisco(
            dma.pistaSeleccionada,
            dma.cilindroSeleccionado,
            dma.sectorSeleccionado,
            bufferSector
        );
        
        if (resultado != 0) {
            // Error al leer del disco
            printf("[DMA] ERROR: Fallo al leer del disco\n");
            dma.estado = 1;  // Error
            dma.ocupado = 0;
            interrupcionPendienteDma = 1;
            return NULL;
        }
        
        // Paso 2: Escribir en memoria (necesita bus)
        // Convertir los 9 caracteres a un entero y guardarlo como Palabra
        // Interpretamos los 9 caracteres como un valor numerico
        valorEntero = 0;
        for (i = 0; i < TAMANO_SECTOR && bufferSector[i] != '\0'; i++) {
            if (bufferSector[i] >= '0' && bufferSector[i] <= '9') {
                valorEntero = valorEntero * 10 + (bufferSector[i] - '0');
            }
        }
        
        // Convertir el entero a Palabra
        palabraTemp = enteroAPalabra(valorEntero);
        
        // Escribir en memoria (usa el semaforo automaticamente)
        escribirMemoria(dma.direccionMemoria, palabraTemp);
        
        printf("[DMA] Datos transferidos a memoria[%d]: %d\n",
               dma.direccionMemoria, valorEntero);
        
        dma.estado = 0;  // Exito
        
    } else {
        // =========================================================
        // ESCRITURA: Memoria -> Disco
        // 1. Leer de memoria (SI usa bus)
        // 2. Escribir en disco (NO usa bus)
        // =========================================================
        
        printf("[DMA] Operacion: ESCRITURA (Memoria -> Disco)\n");
        
        // Paso 1: Leer de memoria (usa el semaforo automaticamente)
        palabraTemp = leerMemoria(dma.direccionMemoria);
        valorEntero = palabraAEntero(palabraTemp);
        
        printf("[DMA] Dato leido de memoria[%d]: %d\n",
               dma.direccionMemoria, valorEntero);
        
        // Convertir el entero a string de 9 caracteres
        // Usamos formato con signo: primer caracter es el signo
        if (valorEntero < 0) {
            bufferSector[0] = '-';
            valorEntero = -valorEntero;
        } else {
            bufferSector[0] = '+';
        }
        
        // Formatear los 8 digitos restantes
        for (i = TAMANO_SECTOR - 1; i >= 1; i--) {
            bufferSector[i] = '0' + (valorEntero % 10);
            valorEntero /= 10;
        }
        
        // Paso 2: Escribir en el disco (no necesita bus)
        resultado = escribirSectorDisco(
            dma.pistaSeleccionada,
            dma.cilindroSeleccionado,
            dma.sectorSeleccionado,
            bufferSector
        );
        
        if (resultado != 0) {
            // Error al escribir en disco
            printf("[DMA] ERROR: Fallo al escribir en disco\n");
            dma.estado = 1;  // Error
            dma.ocupado = 0;
            interrupcionPendienteDma = 1;
            return NULL;
        }
        
        dma.estado = 0;  // Exito
    }
    
    // Marcar DMA como libre
    dma.ocupado = 0;
    
    // Generar interrupcion para el CPU
    interrupcionPendienteDma = 1;
    
    printf("[DMA] Transferencia completada - Estado: %s\n",
           dma.estado == 0 ? "EXITO" : "ERROR");
    printf("[DMA] Interrupcion INT_IO_DONE generada\n");
    
    return NULL;
}

/* ============================================================================
 * IMPLEMENTACION DE FUNCIONES PUBLICAS
 * ============================================================================ */

/*
 * inicializarDma
 * --------------
 * Inicializa todos los registros del DMA a valores por defecto.
 */
void inicializarDma() {
    // Inicializar registros de control a 0
    dma.pistaSeleccionada = 0;
    dma.cilindroSeleccionado = 0;
    dma.sectorSeleccionado = 0;
    dma.direccionIo = 0;
    dma.direccionMemoria = 0;
    
    // Inicializar estado
    dma.estado = 0;      // Sin error
    dma.ocupado = 0;     // Libre
    
    // Limpiar bandera de interrupcion
    interrupcionPendienteDma = 0;
    
    printf("[DMA] Controlador DMA inicializado\n");
}

/*
 * iniciarTransferenciaDma
 * -----------------------
 * Inicia una transferencia creando un nuevo hilo.
 */
void iniciarTransferenciaDma() {
    // Verificar si el DMA ya esta ocupado
    if (dma.ocupado) {
        printf("[DMA] ERROR: DMA ocupado, no se puede iniciar transferencia\n");
        return;
    }
    
    // Marcar DMA como ocupado
    dma.ocupado = 1;
    
    printf("[DMA] Iniciando transferencia en hilo separado...\n");
    
    // Crear hilo para la transferencia
    if (pthread_create(&dma.hiloId, NULL, hiloTransferenciaDma, NULL) != 0) {
        printf("[DMA] ERROR: No se pudo crear el hilo de transferencia\n");
        dma.estado = 1;  // Error
        dma.ocupado = 0;
        interrupcionPendienteDma = 1;
        return;
    }
    
    // Desacoplar el hilo para que se libere automaticamente al terminar
    pthread_detach(dma.hiloId);
}

/*
 * verificarInterrupcionDma
 * ------------------------
 * Verifica si hay una interrupcion pendiente del DMA.
 */
int verificarInterrupcionDma() {
    return interrupcionPendienteDma;
}
