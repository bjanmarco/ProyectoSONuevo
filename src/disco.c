/* ARCHIVO: disco.c
 * DESCRIPCION: Implementacion del modulo de disco magnetico.
 *              Simula un disco con estructura 3D: pista, cilindro, sector.
 *              Segun especificaciones de prueba.txt seccion 4.
 */

#include <stdio.h>
#include <string.h>
#include "../include/disco.h"

// DEFINICION DE VARIABLES GLOBALES

// Disco duro: Arreglo 3D de sectores
// Estructura: [DISCO_CILINDROS][DISCO_PISTAS][DISCO_SECTORES]
// Cada sector contiene 9 caracteres
DiscoDuro discoDuro;

// IMPLEMENTACION DE FUNCIONES

/* inicializarDisco
 * Inicializa todos los sectores del disco con caracteres nulos.
 */
void inicializarDisco() {
    int cilindro, pista, sector, i;
    
    // Recorrer todo el disco e inicializar cada sector
    for (cilindro = 0; cilindro < DISCO_CILINDROS; cilindro++) {
        for (pista = 0; pista < DISCO_PISTAS; pista++) {
            for (sector = 0; sector < DISCO_SECTORES; sector++) {
                // Llenar cada sector con caracteres nulos
                for (i = 0; i < TAMANO_SECTOR; i++) {
                    discoDuro.sectores[cilindro][pista][sector].datos[i] = '\0';
                }
            }
        }
    }
    
    // Imprimir mensaje de log
    printf("[DISCO] Disco inicializado: %d cilindros, %d pistas, %d sectores\n",
           DISCO_CILINDROS, DISCO_PISTAS, DISCO_SECTORES);
    printf("[DISCO] Capacidad total: %d sectores de %d caracteres\n",
           DISCO_CILINDROS * DISCO_PISTAS * DISCO_SECTORES, TAMANO_SECTOR);
}

/* leerSectorDisco
 * Lee los datos de un sector especifico del disco.
 * La comunicacion con el disco NO usa el bus del sistema.
 */
int leerSectorDisco(int pista, int cilindro, int sector, char *buffer) {
    int i;
    
    // Verificar que los parametros sean validos
    if (pista < 0 || pista >= DISCO_PISTAS) {
        printf("[DISCO] ERROR: Pista invalida %d\n", pista);
        return 1; 
    }
    if (cilindro < 0 || cilindro >= DISCO_CILINDROS) {
        printf("[DISCO] ERROR: Cilindro invalido %d\n", cilindro);
        return 1; 
    }
    if (sector < 0 || sector >= DISCO_SECTORES) {
        printf("[DISCO] ERROR: Sector invalido %d\n", sector);
        return 1;  
    }
    if (buffer == NULL) {
        printf("[DISCO] ERROR: Buffer nulo\n");
        return 1;  
    }
    
    // Copiar los datos del sector al buffer
    /* Obtiene un puntero directo al lugar exacto dentro del array 3D donde están los datos. 
     *Es como tener la dirección de una celda en una hoja de cálculo gigante.
    */
    for (i = 0; i < TAMANO_SECTOR; i++) {
        buffer[i] = discoDuro.sectores[cilindro][pista][sector].datos[i];
    }
    
    printf("[DISCO] Lectura exitosa: Cilindro=%d, Pista=%d, Sector=%d\n",
           cilindro, pista, sector);
    
    return 0;  
}

/* escribirSectorDisco
 * Escribe datos en un sector especifico del disco.
 * La comunicacion con el disco NO usa el bus del sistema.
 */
int escribirSectorDisco(int pista, int cilindro, int sector, const char *buffer) {
    int i;
    
    // Verificar que los parametros sean validos
    if (pista < 0 || pista >= DISCO_PISTAS) {
        printf("[DISCO] ERROR: Pista invalida %d\n", pista);
        return 1;  
    }
    if (cilindro < 0 || cilindro >= DISCO_CILINDROS) {
        printf("[DISCO] ERROR: Cilindro invalido %d\n", cilindro);
        return 1; 
    }
    if (sector < 0 || sector >= DISCO_SECTORES) {
        printf("[DISCO] ERROR: Sector invalido %d\n", sector);
        return 1;  
    }
    if (buffer == NULL) {
        printf("[DISCO] ERROR: Buffer nulo\n");
        return 1;  
    }
    
    // Copiar los datos del buffer al sector
    for (i = 0; i < TAMANO_SECTOR; i++) {
        discoDuro.sectores[cilindro][pista][sector].datos[i] = buffer[i];
    }
    
    printf("[DISCO] Escritura exitosa: Cilindro=%d, Pista=%d, Sector=%d\n",
           cilindro, pista, sector);
    
    return 0;
}
