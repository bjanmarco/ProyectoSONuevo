/*
 * ============================================================================
 * ARCHIVO: dma.h
 * DESCRIPCION: Cabecera del modulo DMA (Acceso Directo a Memoria).
 *              Define las funciones para operar el controlador DMA.
 *              Segun especificaciones de prueba.txt seccion 4.
 * ============================================================================
 */

#ifndef DMA_H
#define DMA_H

#include <pthread.h>
#include "hardware.h"

/* ============================================================================
 * VARIABLES GLOBALES DEL DMA
 * ============================================================================ */

// Controlador DMA
extern ControladorDma dma;

// Bandera para indicar que el DMA termino y hay interrupcion pendiente
extern int interrupcionPendienteDma;

/* ============================================================================
 * API DEL DMA (Prototipos de Funciones)
 * ============================================================================ */

/*
 * inicializarDma
 * --------------
 * Inicializa el controlador DMA con valores por defecto.
 * Todos los registros se ponen a cero y el estado a libre.
 */
void inicializarDma();

/*
 * iniciarTransferenciaDma
 * -----------------------
 * Inicia una transferencia de E/S en un hilo separado.
 * El DMA lee los parametros de sus registros internos y ejecuta:
 * - Si direccionIo = 0: Lee del disco y escribe en memoria
 * - Si direccionIo = 1: Lee de memoria y escribe en disco
 * 
 * Al finalizar, establece estado (0=exito, 1=error) y genera
 * interrupcion INT_IO_DONE.
 */
void iniciarTransferenciaDma();

/*
 * verificarInterrupcionDma
 * ------------------------
 * Verifica si el DMA tiene una interrupcion pendiente.
 * 
 * Retorna:
 *   1 si hay interrupcion pendiente, 0 si no
 */
int verificarInterrupcionDma();

#endif /* DMA_H */
