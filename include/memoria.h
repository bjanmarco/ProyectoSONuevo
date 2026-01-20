// ARCHIVO: memoria.h
// Aqui controlamos la RAM de la maquina.
// Lo mas importante es el semaforo del bus, para que CPU y DMA no choquen.

#ifndef MEMORIA_H
#define MEMORIA_H

#include <semaphore.h>
#include "hardware.h"

// --- Constantes Importantes ---
// Copias de lo definido en hardware.h para tenerlo a mano.
// 2000 palabras total, 300 para el SO.

// --- Variables Globales ---

// La RAM propiamente dicha. Un arreglo grandote de Palabras.
extern Palabra memoriaPrincipal[TAMANO_MEMORIA];

// ESTO es vital. Como el DMA corre en otro hilo, podria intentar escribir en RAM
// al mismo tiempo que el CPU lee. Resultado: caos.
// El semaforo actua como un "policia de trafico": solo uno pasa a la vez.
extern sem_t bloqueoBus;

// --- Funciones de Memoria ---

// Prepara el arreglo y, mas importante, inicializa el semaforo en 1 (verde).
void inicializarMemoria();

// Limpieza al apagar. Destruye el semaforo.
void finalizarMemoria();

// Lee una palabra.
// IMPORTANTE: Antes de leer, hace wait() en el semaforo. Si el bus esta ocupado, espera.
// Al terminar, hace post() para liberarlo.
Palabra leerMemoria(int direccion);

// Escribe en RAM. Tambien protegido por el semaforo.
// Si escribes sin permiso (fuera de tu RB/RL), no hace nada (seguridad simulada).
void escribirMemoria(int direccion, Palabra dato);

#endif 
