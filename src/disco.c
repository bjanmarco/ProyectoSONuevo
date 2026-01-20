#include <stdio.h>
#include <string.h>
#include "../include/disco.h"

// Disco duro: Arreglo 3D [cilindros][pistas][sectores], cada sector 9 chars
DiscoDuro discoDuro;

// Funcion auxiliar para validar parametros de disco
static int validarParametrosDisco(int pista, int cilindro, int sector, const void *buffer) {
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
    return 0;
}

void inicializarDisco() {
    int cilindro, pista, sector, i;
    for (cilindro = 0; cilindro < DISCO_CILINDROS; cilindro++) {
        for (pista = 0; pista < DISCO_PISTAS; pista++) {
            for (sector = 0; sector < DISCO_SECTORES; sector++) {
                for (i = 0; i < TAMANO_SECTOR; i++) {
                    discoDuro.sectores[cilindro][pista][sector].datos[i] = '\0';
                }
            }
        }
    }
    printf("[DISCO] Disco inicializado: %d cilindros, %d pistas, %d sectores\n",
           DISCO_CILINDROS, DISCO_PISTAS, DISCO_SECTORES);
    printf("[DISCO] Estructura: %d caracteres por sector\n", TAMANO_SECTOR);
}

int leerSectorDisco(int pista, int cilindro, int sector, char *buffer) {
    int i;
    if (validarParametrosDisco(pista, cilindro, sector, buffer) != 0) return 1;
    for (i = 0; i < TAMANO_SECTOR; i++) {
        buffer[i] = discoDuro.sectores[cilindro][pista][sector].datos[i];
    }
    printf("[DISCO] Lectura exitosa: Cilindro=%d, Pista=%d, Sector=%d\n", cilindro, pista, sector);
    return 0;
}

int escribirSectorDisco(int pista, int cilindro, int sector, const char *buffer) {
    int i;
    if (validarParametrosDisco(pista, cilindro, sector, buffer) != 0) return 1;
    for (i = 0; i < TAMANO_SECTOR; i++) {
        discoDuro.sectores[cilindro][pista][sector].datos[i] = buffer[i];
    }
    printf("[DISCO] Escritura exitosa: Cilindro=%d, Pista=%d, Sector=%d\n", cilindro, pista, sector);
    return 0;
}
