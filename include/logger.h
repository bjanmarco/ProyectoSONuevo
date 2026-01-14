/*
 * ============================================================================
 * ARCHIVO: logger.h
 * DESCRIPCION: Sistema de logging centralizado de la maquina virtual.
 *              Registra todas las acciones del sistema en archivo y consola.
 *              Segun especificaciones de prueba.txt seccion 7.
 * ============================================================================
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

/* ============================================================================
 * NIVELES DE LOG
 * Define la categoria/origen del mensaje de log
 * ============================================================================ */
typedef enum {
    LOG_CPU,            // Mensajes del CPU (ciclos, instrucciones)
    LOG_MEMORIA,        // Mensajes de acceso a memoria
    LOG_DISCO,          // Mensajes de acceso al disco
    LOG_DMA,            // Mensajes del controlador DMA
    LOG_LOADER,         // Mensajes del cargador de programas
    LOG_INTERRUPCION,   // Mensajes de interrupciones (tambien van a stdout)
    LOG_SISTEMA,        // Mensajes generales del sistema
    LOG_DEBUG           // Mensajes de depuracion (modo debug)
} NivelLog;

/* ============================================================================
 * CONSTANTES DEL LOGGER
 * ============================================================================ */

// Nombre del archivo de log por defecto
#define ARCHIVO_LOG_DEFECTO "maquina_virtual.log"

// Tamanio maximo de un mensaje de log
#define MAX_MENSAJE_LOG 512

/* ============================================================================
 * VARIABLES GLOBALES DEL LOGGER
 * ============================================================================ */

// Puntero al archivo de log abierto
extern FILE *archivoLog;

// Indica si el logger esta activo
extern int loggerActivo;

// Contador de lineas de log
extern int contadorLineasLog;

/* ============================================================================
 * API DEL LOGGER (Prototipos de Funciones)
 * ============================================================================ */

/*
 * inicializarLogger
 * -----------------
 * Inicializa el sistema de logging.
 * Abre el archivo de log para escritura.
 * 
 * Parametros:
 *   nombreArchivo - Nombre del archivo de log (NULL para usar defecto)
 * 
 * Retorna:
 *   0 si exito, 1 si error al abrir archivo
 */
int inicializarLogger(const char *nombreArchivo);

/*
 * finalizarLogger
 * ---------------
 * Cierra el sistema de logging.
 * Cierra el archivo de log y libera recursos.
 */
void finalizarLogger();

/*
 * escribirLog
 * -----------
 * Escribe un mensaje en el archivo de log.
 * Los mensajes de tipo LOG_INTERRUPCION tambien se imprimen en stdout.
 * 
 * Parametros:
 *   nivel   - Nivel/categoria del mensaje
 *   formato - Formato del mensaje (como printf)
 *   ...     - Argumentos variables
 */
void escribirLog(NivelLog nivel, const char *formato, ...);

/*
 * logCpu
 * ------
 * Macro para log de CPU (mas conveniente de usar)
 */
#define logCpu(fmt, ...) escribirLog(LOG_CPU, fmt, ##__VA_ARGS__)

/*
 * logMemoria
 * ----------
 * Macro para log de memoria
 */
#define logMemoria(fmt, ...) escribirLog(LOG_MEMORIA, fmt, ##__VA_ARGS__)

/*
 * logDisco
 * --------
 * Macro para log de disco
 */
#define logDisco(fmt, ...) escribirLog(LOG_DISCO, fmt, ##__VA_ARGS__)

/*
 * logDma
 * ------
 * Macro para log de DMA
 */
#define logDma(fmt, ...) escribirLog(LOG_DMA, fmt, ##__VA_ARGS__)

/*
 * logLoader
 * ---------
 * Macro para log del loader
 */
#define logLoader(fmt, ...) escribirLog(LOG_LOADER, fmt, ##__VA_ARGS__)

/*
 * logInterrupcion
 * ---------------
 * Macro para log de interrupciones (tambien va a stdout)
 */
#define logInterrupcion(fmt, ...) escribirLog(LOG_INTERRUPCION, fmt, ##__VA_ARGS__)

/*
 * logSistema
 * ----------
 * Macro para log del sistema
 */
#define logSistema(fmt, ...) escribirLog(LOG_SISTEMA, fmt, ##__VA_ARGS__)

/*
 * logDebug
 * --------
 * Macro para log de depuracion
 */
#define logDebug(fmt, ...) escribirLog(LOG_DEBUG, fmt, ##__VA_ARGS__)

#endif /* LOGGER_H */
