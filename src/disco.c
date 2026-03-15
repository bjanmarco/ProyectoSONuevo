// Simulacion en RAM de las estructuras de disco duro
#include <stdio.h>
#include <string.h>
#include "../include/disco.h"
#include "../include/logger.h"

// Disco duro simulado
DiscoDuro discoDuro;

static int validarParametrosDisco(int pista, int cilindro, int sector, const void *buffer) {
    if (pista < 0 || pista >= DISCO_PISTAS) {
        logDisco("ERROR: Pista invalida %d", pista);
        return 1;
    }
    if (cilindro < 0 || cilindro >= DISCO_CILINDROS) {
        logDisco("ERROR: Cilindro invalido %d", cilindro);
        return 1;
    }
    if (sector < 0 || sector >= DISCO_SECTORES) {
        logDisco("ERROR: Sector invalido %d", sector);
        return 1;
    }
    if (buffer == NULL) {
        logDisco("ERROR: Buffer nulo");
        return 1;
    }
    return 0;
}

// Inicializacion del arreglo
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
    logDisco("Disco inicializado: %d cilindros, %d pistas, %d sectores",
           DISCO_CILINDROS, DISCO_PISTAS, DISCO_SECTORES);
    logDisco("Estructura: %d caracteres por sector", TAMANO_SECTOR);
}

int leerSectorDisco(int pista, int cilindro, int sector, char *buffer) {
    int i;
    if (validarParametrosDisco(pista, cilindro, sector, buffer) != 0) return 1;
    for (i = 0; i < TAMANO_SECTOR; i++) {
        buffer[i] = discoDuro.sectores[cilindro][pista][sector].datos[i];
    }
    logDisco("Lectura exitosa: Cilindro=%d, Pista=%d, Sector=%d", cilindro, pista, sector);
    return 0;
}

int escribirSectorDisco(int pista, int cilindro, int sector, const char *buffer) {
    int i;
    if (validarParametrosDisco(pista, cilindro, sector, buffer) != 0) return 1;
    for (i = 0; i < TAMANO_SECTOR; i++) {
        discoDuro.sectores[cilindro][pista][sector].datos[i] = buffer[i];
    }
    logDisco("Escritura exitosa: Cilindro=%d, Pista=%d, Sector=%d", cilindro, pista, sector);
    return 0;
}
