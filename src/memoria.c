#include <stdio.h>
#include <string.h>
#include "../include/memoria.h"

// Arreglo de memoria principal
Palabra memoriaPrincipal[TAMANO_MEMORIA];

// Semaforo para arbitraje del bus, para evitar las coniciones de carrera
sem_t bloqueoBus;

// Inicializa la memoria principal del sistema.
void inicializarMemoria() {
    int i; // contador
    for (i = 0; i < TAMANO_MEMORIA; i++) {
        memoriaPrincipal[i].signo = 0;
        memoriaPrincipal[i].digitos = 0; // pasa desde  0 hasta 2000 para inicializar la memoria y que esta vacia
    }
    sem_init(&bloqueoBus, 0, 1); // inicializa el semaforo para que pueda usar el bus
    printf("[MEMORIA] Inicializada: %d posiciones, SO: 0-%d, Usuario: %d-%d\n", 
           TAMANO_MEMORIA, TAMANO_MEMORIA_SO - 1, INICIO_MEMORIA_USUARIO, TAMANO_MEMORIA - 1);
}

// Finaliza el uso de la memoria y libera recursos.
void finalizarMemoria() {
    sem_destroy(&bloqueoBus); // destruye el semáforo de bloqueo del bus utilizado para la exclusión mutua
    printf("[MEMORIA] Recursos liberados\n");
}

// Lee una palabra de la memoria en la dirección especificada.
Palabra leerMemoria(int direccion) {
    Palabra resultado = {0, 0};
    if (direccion < 0 || direccion >= TAMANO_MEMORIA) {
        printf("[MEMORIA] ERROR: Lectura en direccion invalida %d\n", direccion);
        return resultado;
    }
    sem_wait(&bloqueoBus); // espera a que el semaforo este disponible
    resultado = memoriaPrincipal[direccion];
    sem_post(&bloqueoBus); // libera el semaforo
    return resultado;
}

// Escribe una palabra en la memoria en la dirección especificada.
void escribirMemoria(int direccion, Palabra dato) {
    if (direccion < 0 || direccion >= TAMANO_MEMORIA) {
        printf("[MEMORIA] ERROR: Escritura en direccion invalida %d\n", direccion);
        return;
    }
    sem_wait(&bloqueoBus); 
    memoriaPrincipal[direccion] = dato;
    sem_post(&bloqueoBus);
}
