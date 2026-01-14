/*
 * ============================================================================
 * ARCHIVO: disco.h
 * DESCRIPCION: Cabecera del modulo de disco magnetico de la maquina virtual.
 *              Define las funciones para acceder al disco de almacenamiento.
 *              Segun especificaciones de prueba.txt seccion 4.
 * ============================================================================
 */

#ifndef DISCO_H
#define DISCO_H

#include "hardware.h"

/* ============================================================================
 * CONSTANTES DE DISCO
 * Ya definidas en hardware.h:
 * - DISCO_CILINDROS = 10
 * - DISCO_PISTAS = 10
 * - DISCO_SECTORES = 100
 * - TAMANO_SECTOR = 9 caracteres
 * ============================================================================ */

/* ============================================================================
 * VARIABLES GLOBALES DEL DISCO
 * ============================================================================ */

// Disco duro: Arreglo 3D [cilindro][pista][sector]
extern DiscoDuro discoDuro;

/* ============================================================================
 * API DEL DISCO (Prototipos de Funciones)
 * ============================================================================ */

/*
 * inicializarDisco
 * ----------------
 * Inicializa el disco magnetico con datos vacios.
 * Todos los sectores se llenan con caracteres nulos.
 */
void inicializarDisco();

/*
 * leerSectorDisco
 * ---------------
 * Lee los datos de un sector del disco.
 * 
 * Parametros:
 *   pista    - Numero de pista (0 a 9)
 *   cilindro - Numero de cilindro (0 a 9)
 *   sector   - Numero de sector (0 a 99)
 *   buffer   - Buffer donde se copiaran los 9 caracteres
 * 
 * Retorna:
 *   0 si exito, 1 si error (parametros invalidos)
 */
int leerSectorDisco(int pista, int cilindro, int sector, char *buffer);

/*
 * escribirSectorDisco
 * -------------------
 * Escribe datos en un sector del disco.
 * 
 * Parametros:
 *   pista    - Numero de pista (0 a 9)
 *   cilindro - Numero de cilindro (0 a 9)
 *   sector   - Numero de sector (0 a 99)
 *   buffer   - Buffer con los 9 caracteres a escribir
 * 
 * Retorna:
 *   0 si exito, 1 si error (parametros invalidos)
 */
int escribirSectorDisco(int pista, int cilindro, int sector, const char *buffer);

#endif /* DISCO_H */
