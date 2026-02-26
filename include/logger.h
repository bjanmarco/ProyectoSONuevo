// básicamente un chismoso que anota todo lo que pasa en "maquina_virtual.log".
// Es vital para encontrar bugs sin volverse loco con prints en consola
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

// Este es un enum para identificar los nivles porque asi se puede saber quien escribe
typedef enum {
    LOG_CPU,            // para el CPU, ciclos e instrucciones
    LOG_MEMORIA,        // para la memoria, lecturas y escrituras
    LOG_DISCO,          // para el disco, lecturas y escrituras
    LOG_DMA,            // para el DMA  , lecturas y escrituras
    LOG_LOADER,         // para el loader, lecturas y escrituras
    LOG_INTERRUPCION,   // este tambien sale por pantalla porque es importante
    LOG_SISTEMA,        // para el sistema, errores y advertencias
    LOG_DEBUG           // para el debug, solo sale por pantalla
} NivelLog;

// constantes importantes
#define ARCHIVO_LOG_DEFECTO "maquinaVirtual.log"
#define MAX_MENSAJE_LOG 512

// variables globales 
extern FILE *archivoLog;      // el archivo fisico donde escribimos
extern int loggerActivo;      // variable para apagarlo

// funciones del logger
// abre el archivo y si falla (por permisos o disco lleno), avisa.
int inicializarLogger(const char *nombreArchivo);

// cierra el archivo al salir.
void finalizarLogger();

// la funcion base. funciona igual que printf pero requiere el del log (nivel).
void escribirLog(NivelLog nivel, const char *formato, ...);

// usamos macros como atajos, que son como funciones de buscar y remplazar
// todo para no tener que escribir "LOG_CPU" todo el tiempo.
#define logCpu(fmt, ...) escribirLog(LOG_CPU, fmt, ##__VA_ARGS__)
#define logMemoria(fmt, ...) escribirLog(LOG_MEMORIA, fmt, ##__VA_ARGS__)
#define logDisco(fmt, ...) escribirLog(LOG_DISCO, fmt, ##__VA_ARGS__)
#define logDma(fmt, ...) escribirLog(LOG_DMA, fmt, ##__VA_ARGS__)
#define logLoader(fmt, ...) escribirLog(LOG_LOADER, fmt, ##__VA_ARGS__)
#define logInterrupcion(fmt, ...) escribirLog(LOG_INTERRUPCION, fmt, ##__VA_ARGS__)
#define logSistema(fmt, ...) escribirLog(LOG_SISTEMA, fmt, ##__VA_ARGS__)
#define logDebug(fmt, ...) escribirLog(LOG_DEBUG, fmt, ##__VA_ARGS__)

#endif
