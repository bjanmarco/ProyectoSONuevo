#include <stdio.h>
#include <string.h>
#include "../include/memoria.h"
#include "../include/procesos.h"
#include "../include/logger.h"

// RAM Principal
Palabra memoriaPrincipal[TAMANO_MEMORIA];

// Semaforo Bus
sem_t bloqueoBus;

// Inicializar RAM
void inicializarMemoria() {
    int i;
    for (i = 0; i < TAMANO_MEMORIA; i++) {
        memoriaPrincipal[i].signo = 0;
        memoriaPrincipal[i].digitos = 0;
    }
    sem_init(&bloqueoBus, 0, 1);
    logMemoria("Inicializada: %d posiciones, SO: %d-%d, Usuario: %d-%d", 
           TAMANO_MEMORIA, 0, TAMANO_MEMORIA_SO - 1, INICIO_MEMORIA_USUARIO, TAMANO_MEMORIA - 1);
}

// Finalizar memoria
void finalizarMemoria() {
    sem_destroy(&bloqueoBus);
    logMemoria("Recursos liberados");
}

// Leer palabra
Palabra leerMemoria(int direccion) {
    Palabra resultado = {0, 0};
    if (direccion < 0 || direccion >= TAMANO_MEMORIA) {
        logMemoria("ERROR: Lectura en direccion invalida %d", direccion);
        return resultado;
    }

    sem_wait(&bloqueoBus);
    resultado = memoriaPrincipal[direccion];
    sem_post(&bloqueoBus);
    return resultado;
}

// Escribir palabra
void escribirMemoria(int direccion, Palabra dato) {
    if (direccion < 0 || direccion >= TAMANO_MEMORIA) {
        logMemoria("ERROR: Escritura en direccion invalida %d", direccion);
        return;
    }

    sem_wait(&bloqueoBus);
    memoriaPrincipal[direccion] = dato;
    sem_post(&bloqueoBus);
}

// Funciones de Diagnostico

void mostrarEstadisticasMemoria() {
    int celdasOcupadasSO = 0;
    int celdasOcupadasUsuario = 0;

    for (int i = 0; i < TAMANO_MEMORIA; i++) {
        // Celda ocupada si es != 0
        if (memoriaPrincipal[i].signo != 0 || memoriaPrincipal[i].digitos != 0) {
            if (i < TAMANO_MEMORIA_SO) {
                celdasOcupadasSO++;
            } else {
                celdasOcupadasUsuario++;
            }
        }
    }

    int totalOcupadas = celdasOcupadasSO + celdasOcupadasUsuario;
    float porcentajeUso = ((float)totalOcupadas / TAMANO_MEMORIA) * 100.0f;

    printf("\nESTADISTICAS DE MEMORIA (memstat)\n");
    printf("================================================================================\n");
    printf("Tamano Total de RAM        : %d Palabras\n", TAMANO_MEMORIA);
    printf("Zona del Kernel (0-%d)    : %d Palabras | Posiciones ocupadas: %d\n", TAMANO_MEMORIA_SO - 1, TAMANO_MEMORIA_SO, celdasOcupadasSO);
    printf("Zona de Usuario (%d-%d) : %d Palabras | Posiciones ocupadas: %d\n", INICIO_MEMORIA_USUARIO, TAMANO_MEMORIA - 1, TAMANO_MEMORIA - TAMANO_MEMORIA_SO, celdasOcupadasUsuario);
    printf("Porcentaje de Uso Total    : %.2f%%\n\n", porcentajeUso);
}
