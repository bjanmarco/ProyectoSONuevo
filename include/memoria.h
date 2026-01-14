/*
 * ============================================================================
 * ARCHIVO: memoria.h
 * DESCRIPCION: Cabecera del modulo de memoria RAM de la maquina virtual.
 *              Define la estructura y funciones para acceder a la memoria.
 *              Segun especificaciones de prueba.txt seccion 2.
 * ============================================================================
 */

#ifndef MEMORIA_H
#define MEMORIA_H

#include <semaphore.h>
#include "hardware.h"

/* ============================================================================
 * CONSTANTES DE MEMORIA
 * Ya definidas en hardware.h, pero documentadas aqui:
 * - TAMANO_MEMORIA = 2000 (capacidad total)
 * - TAMANO_MEMORIA_SO = 300 (reservado para SO, posiciones 0-299)
 * - INICIO_MEMORIA_USUARIO = 300 (inicio del espacio de usuario)
 * ============================================================================ */

/* ============================================================================
 * VARIABLES GLOBALES DE MEMORIA
 * Declaradas como extern para ser definidas en memoria.c
 * ============================================================================ */

// Arreglo principal de memoria: 2000 palabras de 8 digitos
extern Palabra memoriaPrincipal[TAMANO_MEMORIA];

// Semaforo para el arbitraje del bus del sistema
// Controla el acceso concurrente entre CPU y DMA
extern sem_t bloqueoBus;

/* ============================================================================
 * API DE MEMORIA (Prototipos de Funciones)
 * Todas las funciones usan camelCase segun requisitos
 * ============================================================================ */

/*
 * inicializarMemoria
 * ------------------
 * Inicializa la memoria RAM y el semaforo del bus.
 * - Pone todas las posiciones de memoria a cero.
 * - Inicializa el semaforo bloqueoBus con valor 1 (binario).
 */
void inicializarMemoria();

/*
 * finalizarMemoria
 * ----------------
 * Libera los recursos de la memoria.
 * - Destruye el semaforo del bus.
 */
void finalizarMemoria();

/*
 * leerMemoria
 * -----------
 * Lee una palabra de la posicion de memoria especificada.
 * Usa el semaforo para garantizar acceso exclusivo al bus.
 * 
 * Parametros:
 *   direccion - La direccion de memoria a leer (0 a 1999)
 * 
 * Retorna:
 *   La Palabra almacenada en esa direccion.
 *   Si la direccion es invalida, retorna una palabra con valor 0.
 */
Palabra leerMemoria(int direccion);

/*
 * escribirMemoria
 * ---------------
 * Escribe una palabra en la posicion de memoria especificada.
 * Usa el semaforo para garantizar acceso exclusivo al bus.
 * 
 * Parametros:
 *   direccion - La direccion de memoria donde escribir (0 a 1999)
 *   dato      - La Palabra a almacenar en esa direccion
 */
void escribirMemoria(int direccion, Palabra dato);

#endif /* MEMORIA_H */
