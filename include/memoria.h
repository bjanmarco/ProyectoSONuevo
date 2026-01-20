#ifndef MEMORIA_H
#define MEMORIA_H

#include <semaphore.h>
#include "hardware.h"

// constantes importantes 
// copias de lo definido en hardware.h para tenerlo a mano
// 2000 palabras total, 300 para el SO

// variables globales 

// la RAM Un arreglo grande de Palabras
extern Palabra memoriaPrincipal[TAMANO_MEMORIA];

// este es el semaforo para evitar condiciones de carrera CPU/DMA
extern sem_t bloqueoBus;

// Funciones de Memoria

// prepara el arreglo e inicializa el semaforo en 1 (verde)
void inicializarMemoria();

// limpieza finalizar y destruye el semaforo
void finalizarMemoria();

// lee una palabra
// antes de leer hace wait() en el semaforo y si el bus esta ocupado, espera
// al terminar hace post() para liberarlo
Palabra leerMemoria(int direccion);

// escribe en la memoria
// si escribes sin permiso (fuera de tu RB/RL) no hace nada
void escribirMemoria(int direccion, Palabra dato);

#endif 
