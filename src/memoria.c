#include <stdio.h>
#include <string.h>
#include "../include/memoria.h"

// Arreglo de memoria principal: 2000 palabras (0-299 SO, 300-1999 Usuario)
Palabra memoriaPrincipal[TAMANO_MEMORIA];

// Semaforo para arbitraje del bus (evita condiciones de carrera CPU/DMA)
sem_t bloqueoBus;

void inicializarMemoria() {
    int i;
    for (i = 0; i < TAMANO_MEMORIA; i++) {
        memoriaPrincipal[i].signo = 0;
        memoriaPrincipal[i].digitos = 0;
    }
    sem_init(&bloqueoBus, 0, 1);
    printf("[MEMORIA] Inicializada: %d posiciones, SO: 0-%d, Usuario: %d-%d\n", 
           TAMANO_MEMORIA, TAMANO_MEMORIA_SO - 1, INICIO_MEMORIA_USUARIO, TAMANO_MEMORIA - 1);
}

void finalizarMemoria() {
    sem_destroy(&bloqueoBus);
    printf("[MEMORIA] Recursos liberados\n");
}

Palabra leerMemoria(int direccion) {
    Palabra resultado = {0, 0};
    if (direccion < 0 || direccion >= TAMANO_MEMORIA) {
        printf("[MEMORIA] ERROR: Lectura en direccion invalida %d\n", direccion);
        return resultado;
    }
    sem_wait(&bloqueoBus);
    resultado = memoriaPrincipal[direccion];
    sem_post(&bloqueoBus);
    return resultado;
}

void escribirMemoria(int direccion, Palabra dato) {
    if (direccion < 0 || direccion >= TAMANO_MEMORIA) {
        printf("[MEMORIA] ERROR: Escritura en direccion invalida %d\n", direccion);
        return;
    }
    sem_wait(&bloqueoBus);
    memoriaPrincipal[direccion] = dato;
    sem_post(&bloqueoBus);
}
