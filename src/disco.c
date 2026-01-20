#include <stdio.h>
#include <string.h>
#include "../include/disco.h"

// Arreglo de 3 niveles [cilindros][pistas][sectores], cada sector 9 char
DiscoDuro discoDuro;

// Funcion auxiliar para validar parametros de disco
static int validarParametrosDisco(int pista, int cilindro, int sector, const void *buffer) {
    if (pista < 0 || pista >= DISCO_PISTAS) {
        // si no esta dentro del rango sale y muestra error
        printf("[DISCO] ERROR: Pista invalida %d\n", pista);
        return 1;
    }
    if (cilindro < 0 || cilindro >= DISCO_CILINDROS) {
        // si no esta dentro del rango sale y muestra error
        printf("[DISCO] ERROR: Cilindro invalido %d\n", cilindro);
        return 1;
    }
    if (sector < 0 || sector >= DISCO_SECTORES) {
        // si no esta dentro del rango sale y muestra error
        printf("[DISCO] ERROR: Sector invalido %d\n", sector);
        return 1;
    }
    if (buffer == NULL) {
        printf("[DISCO] ERROR: Buffer nulo\n");
        return 1;
    }
    return 0;
}

// Es parecido como con l memoria se va recorriendo cada espacio para ver si esta vacio o no
void inicializarDisco() {
    int cilindro, pista, sector, i; // se declaran las variables
    for (cilindro = 0; cilindro < DISCO_CILINDROS; cilindro++) { // desde el nivel mas grande
        for (pista = 0; pista < DISCO_PISTAS; pista++) {
            for (sector = 0; sector < DISCO_SECTORES; sector++) {
                for (i = 0; i < TAMANO_SECTOR; i++) { // hast el nivel mas pequeño
                    discoDuro.sectores[cilindro][pista][sector].datos[i] = '\0';
                }
            }
        }
    }
    printf("[DISCO] Disco inicializado: %d cilindros, %d pistas, %d sectores\n",
           DISCO_CILINDROS, DISCO_PISTAS, DISCO_SECTORES);
    printf("[DISCO] Capacidad total: %d sectores de %d caracteres\n",
           DISCO_CILINDROS * DISCO_PISTAS * DISCO_SECTORES, TAMANO_SECTOR);
}
// Hay que verlo como for anidados para poder recorrerlo, porqu eun nivel compone a los de arriba

int leerSectorDisco(int pista, int cilindro, int sector, char *buffer) {
    int i;
    if (validarParametrosDisco(pista, cilindro, sector, buffer) != 0) return 1;
    for (i = 0; i < TAMANO_SECTOR; i++) {
        buffer[i] = discoDuro.sectores[cilindro][pista][sector].datos[i];
    } // si los parametros son validos, recorremos el sector y copiamos los datos
    printf("[DISCO] Lectura exitosa: Cilindro=%d, Pista=%d, Sector=%d\n", cilindro, pista, sector);
    return 0;
}

int escribirSectorDisco(int pista, int cilindro, int sector, const char *buffer) {
    int i;
    if (validarParametrosDisco(pista, cilindro, sector, buffer) != 0) return 1;
    for (i = 0; i < TAMANO_SECTOR; i++) {
        discoDuro.sectores[cilindro][pista][sector].datos[i] = buffer[i];
    } // si los parametros son validos, recorremos el sector y copiamos los datos
    printf("[DISCO] Escritura exitosa: Cilindro=%d, Pista=%d, Sector=%d\n", cilindro, pista, sector);
    return 0;
}
