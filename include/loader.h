/*
 * ============================================================================
 * ARCHIVO: loader.h
 * DESCRIPCION: Cabecera del cargador de programas de la maquina virtual.
 *              Define las funciones para cargar programas desde archivos.
 *              Segun especificaciones de prueba.txt seccion 6.
 * ============================================================================
 */

#ifndef LOADER_H
#define LOADER_H

#include "hardware.h"

/* ============================================================================
 * CONSTANTES DEL LOADER
 * ============================================================================ */

// Tamanio maximo del nombre de un programa
#define MAX_NOMBRE_PROGRAMA 50

// Tamanio maximo de una linea del archivo
#define MAX_LINEA 100

/* ============================================================================
 * ESTRUCTURA DE INFORMACION DE PROGRAMA
 * Almacena los datos del programa cargado
 * ============================================================================ */
typedef struct {
    char nombre[MAX_NOMBRE_PROGRAMA];   // Nombre del programa
    int lineaInicio;                    // Valor de _start (donde empieza ejecucion)
    int numeroPalabras;                 // Cantidad de instrucciones
    int direccionBase;                  // RB asignado (direccion fisica)
    int direccionLimite;                // RL asignado (direccion fisica)
} InfoPrograma;

/* ============================================================================
 * VARIABLES GLOBALES DEL LOADER
 * ============================================================================ */

// Siguiente direccion de memoria disponible para cargar programas
// Inicia en INICIO_MEMORIA_USUARIO (300) y se incrementa
extern int siguienteDireccionDisponible;

// Informacion del ultimo programa cargado
extern InfoPrograma programaActual;

/* ============================================================================
 * API DEL LOADER (Prototipos de Funciones)
 * ============================================================================ */

/*
 * inicializarLoader
 * -----------------
 * Inicializa el loader.
 * Establece la siguiente direccion disponible a INICIO_MEMORIA_USUARIO (300).
 */
void inicializarLoader();

/*
 * cargarPrograma
 * ---------------
 * Carga un programa desde un archivo a memoria.
 * 
 * El formato del archivo es:
 *   _start <linea de inicio>
 *   .NumeroPalabras <cantidad>
 *   .NombreProg <nombre>
 *   <instrucciones en formato 8 digitos>
 *   .
 * 
 * Parametros:
 *   rutaArchivo - Ruta al archivo del programa
 * 
 * Retorna:
 *   0 si se cargo exitosamente
 *   1 si hubo error (archivo no existe, formato invalido, etc.)
 * 
 * Efectos:
 *   - Carga las instrucciones en memoria a partir de siguienteDireccionDisponible
 *   - Actualiza registrosCpu.rb, rl, psw.pc
 *   - Actualiza programaActual con la info del programa
 *   - Incrementa siguienteDireccionDisponible
 */
int cargarPrograma(const char *rutaArchivo);

/*
 * prepararEjecucion
 * -----------------
 * Prepara el CPU para ejecutar el programa cargado.
 * Establece PC, RB, RL, SP segun el programa actual.
 */
void prepararEjecucion();

/*
 * reiniciarLoader
 * ---------------
 * Reinicia el loader al estado inicial.
 * Vuelve a poner siguienteDireccionDisponible en 300.
 * Util para reiniciar la maquina virtual.
 */
void reiniciarLoader();

#endif /* LOADER_H */
