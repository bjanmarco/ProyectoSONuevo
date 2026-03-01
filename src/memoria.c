#include <stdio.h>
#include <string.h>
#include "../include/memoria.h"

// arreglo de memoria principal que son 2000 palabras (0-299 SO, 300-1999 Usuario)
Palabra memoriaPrincipal[TAMANO_MEMORIA];

// semaforo para arbitraje del bus
sem_t bloqueoBus;

// inicializa la memoria principal del sistema
void inicializarMemoria() {
    int i;
    for (i = 0; i < TAMANO_MEMORIA; i++) { // limpia todas las posiciones de memoria estableciendo el signo y los dígitos a 0
        memoriaPrincipal[i].signo = 0;     // porque asi nos aseguramos que no haya basura
        memoriaPrincipal[i].digitos = 0;
    }
    sem_init(&bloqueoBus, 0, 1); // inicializa el semáforo para el control de acceso al bus de datos
    printf("[MEMORIA] Inicializada: %d posiciones, SO: 0-%d, Usuario: %d-%d\n", 
           TAMANO_MEMORIA, TAMANO_MEMORIA_SO - 1, INICIO_MEMORIA_USUARIO, TAMANO_MEMORIA - 1);
            // muestra un mensaje sobre el tamaño y partición de la memoria
}

// finaliza el uso de la memoria y libera recursos
void finalizarMemoria() {
    sem_destroy(&bloqueoBus); // destruye el semáforo de bloqueo del bus utilizado para la exclusión mutua.
    printf("[MEMORIA] Recursos liberados\n");
}

// lee una palabra de la memoria en la dirección especificada.
Palabra leerMemoria(int direccion) {
    Palabra resultado = {0, 0};
    if (direccion < 0 || direccion >= TAMANO_MEMORIA) { // valida que la dirección esté dentro de los límites
        printf("[MEMORIA] ERROR: Lectura en direccion invalida %d\n", direccion);
        return resultado;
    }
    // utiliza un semáforo para garantizar acceso exclusivo al bus durante la lectura
    sem_wait(&bloqueoBus); // espera a que el semáforo esté disponible
    resultado = memoriaPrincipal[direccion];
    sem_post(&bloqueoBus); // libera el semáforo
    return resultado;
}
// escribe una palabra en la memoria en la dirección especificada
void escribirMemoria(int direccion, Palabra dato) {
    if (direccion < 0 || direccion >= TAMANO_MEMORIA) { // valida que la dirección esté dentro de los límites
        printf("[MEMORIA] ERROR: Escritura en direccion invalida %d\n", direccion);
        return;
    }
    // utiliza un semáforo para garantizar acceso exclusivo al bus durante la escritura
    sem_wait(&bloqueoBus); // espera a que el semáforo esté disponible
    memoriaPrincipal[direccion] = dato;
    sem_post(&bloqueoBus); // libera el semáforo
}

// ============================================
// Funciones Diagnostico (Comandos Fase 2)
// ============================================

void mostrarEstadisticasMemoria() {
    int celdasOcupadasSO = 0;
    int celdasOcupadasUsuario = 0;

    for (int i = 0; i < TAMANO_MEMORIA; i++) {
        // "Ocupada" = si el opcode no es 0
        if (palabraAEntero(memoriaPrincipal[i]) != 0) {
            if (i < TAMANO_MEMORIA_SO) {
                celdasOcupadasSO++;
            } else {
                celdasOcupadasUsuario++;
            }
        }
    }

    int totalOcupadas = celdasOcupadasSO + celdasOcupadasUsuario;
    float porcentajeUso = ((float)totalOcupadas / TAMANO_MEMORIA) * 100.0f;

    printf("\n=== ESTADISTICAS DE MEMORIA (memestat) ===\n");
    printf("Tamano Total de RAM : %d Palabras\n", TAMANO_MEMORIA);
    printf("Tamano Particion SO : %d Palabras | Posiciones con Dato: %d\n", TAMANO_MEMORIA_SO, celdasOcupadasSO);
    printf("Tamano Zona Usuario : %d Palabras | Posiciones con Dato: %d\n", TAMANO_MEMORIA - TAMANO_MEMORIA_SO, celdasOcupadasUsuario);
    printf("Porcentaje de Uso   : %.2f%%\n", porcentajeUso);
    printf("==========================================\n\n");
}
