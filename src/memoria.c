/* ARCHIVO: memoria.c
 * DESCRIPCION: Implementacion del modulo de memoria RAM de la maquina virtual.
 *              Contiene las funciones para inicializar, leer y escribir
 *              en la memoria principal con control de acceso al bus.
 *              Segun especificaciones de prueba.txt seccion 2 y 4.
 */

#include <stdio.h>
#include <string.h>
#include "../include/memoria.h"

// DEFINICION DE VARIABLES GLOBALES 

// Arreglo de memoria principal: 2000 palabras
// Posiciones 0-299: Reservadas para el Sistema Operativo
// Posiciones 300-1999: Espacio de Usuario
Palabra memoriaPrincipal[TAMANO_MEMORIA];

// Semaforo binario para el arbitraje del bus del sistema
// Evita condiciones de carrera entre CPU y DMA al acceder a memoria
sem_t bloqueoBus;

// IMPLEMENTACION DE FUNCIONES

/* inicializarMemoria
 * Inicializa toda la memoria RAM a cero y el semaforo del bus.
 * Esta funcion debe llamarse antes de usar cualquier otra funcion de memoria.
 */
void inicializarMemoria() {
    int i;
    
    // Inicializar todas las posiciones de memoria a cero
    // Cada palabra tiene signo 0 (positivo) y digitos 0
    for (i = 0; i < TAMANO_MEMORIA; i++) {
        memoriaPrincipal[i].signo = 0;      // Signo positivo
        memoriaPrincipal[i].digitos = 0;    // Valor cero
    }
    
    // Inicializar el semaforo del bus con valor 1 (disponible)
    // Esto permite que un solo proceso acceda al bus a la vez
    sem_init(&bloqueoBus, 0, 1);
    
    // Imprimir mensaje de log
    printf("[MEMORIA] Memoria inicializada: %d posiciones\n", TAMANO_MEMORIA);
    printf("[MEMORIA] Area SO: 0-%d, Area Usuario: %d-%d\n",  TAMANO_MEMORIA_SO - 1, INICIO_MEMORIA_USUARIO, TAMANO_MEMORIA - 1);
    printf("[MEMORIA] Semaforo del bus inicializado\n");
}

/* finalizarMemoria
 * Libera los recursos de la memoria, especificamente el semaforo.
 * Debe llamarse al finalizar el programa para evitar fugas de recursos.
 */
void finalizarMemoria() {
    // Destruir el semaforo del bus
    sem_destroy(&bloqueoBus);
    printf("[MEMORIA] Recursos de memoria liberados\n");
}

/* leerMemoria
 * Lee una palabra de la direccion de memoria especificada.
 * Adquiere el semaforo del bus antes de leer para evitar conflictos.
 * Parametros:
 *   direccion - Direccion de memoria a leer (0 a 1999)
 * Retorna:
 *   La Palabra en esa direccion, o una palabra cero si es invalida.
 */
Palabra leerMemoria(int direccion) {
    Palabra resultado;
    
    // Verificar que la direccion sea valida
    if (direccion < 0 || direccion >= TAMANO_MEMORIA) {
        // Direccion fuera de rango, retornar palabra cero
        printf("[MEMORIA] ERROR: Lectura en direccion invalida %d\n", direccion);
        resultado.signo = 0;
        resultado.digitos = 0;
        return resultado;
    }
    
    // Adquirir el semaforo del bus (espera si esta ocupado)
    sem_wait(&bloqueoBus);
    
    // Leer el dato de la memoria
    resultado = memoriaPrincipal[direccion];
    
    // Liberar el semaforo del bus
    sem_post(&bloqueoBus);
    
    return resultado;
}

/* escribirMemoria
 * Escribe una palabra en la direccion de memoria especificada.
 * Adquiere el semaforo del bus antes de escribir para evitar conflictos.
 * Parametros:
 *   direccion - Direccion de memoria donde escribir (0 a 1999)
 *   dato      - La Palabra a almacenar
 */
void escribirMemoria(int direccion, Palabra dato) {
    // Verificar que la direccion sea valida
    if (direccion < 0 || direccion >= TAMANO_MEMORIA) {
        // Direccion fuera de rango, no hacer nada
        printf("[MEMORIA] ERROR: Escritura en direccion invalida %d\n", direccion);
        return;
    }
    
    // Adquirir el semaforo del bus (espera si esta ocupado)
    sem_wait(&bloqueoBus);
    
    // Escribir el dato en memoria
    memoriaPrincipal[direccion] = dato;
    
    // Liberar el semaforo del bus
    sem_post(&bloqueoBus);
}
