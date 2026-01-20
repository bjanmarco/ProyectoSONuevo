// ARCHIVO: logger.h
// El sistema de Logs (Bitacora).
// Basicamente, un chismoso que anota TODO lo que pasa en "maquina_virtual.log".
// Es vital para encontrar bugs sin volverse loco con prints en consola.

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

// Niveles de log para saber quien esta hablando.
typedef enum {
    LOG_CPU,            
    LOG_MEMORIA,        
    LOG_DISCO,          
    LOG_DMA,            
    LOG_LOADER,         
    LOG_INTERRUPCION,   // Este es especial: tambien sale por pantalla porque es importante.
    LOG_SISTEMA,        
    LOG_DEBUG           
} NivelLog;

#define ARCHIVO_LOG_DEFECTO "maquina_virtual.log"
#define MAX_MENSAJE_LOG 512

// --- Variables Globales del Logger ---

extern FILE *archivoLog;      // El archivo fisico donde escribimos
extern int loggerActivo;      // Switch maestro por si queremos silenciarlo
extern int contadorLineasLog; // Pa saber cuanto escribimos

// --- Funciones del Logger ---

// Abre el archivo. Si falla (por permisos o disco lleno), avisa.
int inicializarLogger(const char *nombreArchivo);

// Cierra el archivo educadamente al salir.
void finalizarLogger();

// La funcion base. Funciona igual que printf pero requiere que le digas quien eres (nivel).
void escribirLog(NivelLog nivel, const char *formato, ...);

// --- Macros utiles (Atajos) ---
// Usamos macros para no tener que escribir "LOG_CPU" todo el tiempo.
// Son wrappers cosmeticos sobre escribirLog.

#define logCpu(fmt, ...) escribirLog(LOG_CPU, fmt, ##__VA_ARGS__)
#define logMemoria(fmt, ...) escribirLog(LOG_MEMORIA, fmt, ##__VA_ARGS__)
#define logDisco(fmt, ...) escribirLog(LOG_DISCO, fmt, ##__VA_ARGS__)
#define logDma(fmt, ...) escribirLog(LOG_DMA, fmt, ##__VA_ARGS__)
#define logLoader(fmt, ...) escribirLog(LOG_LOADER, fmt, ##__VA_ARGS__)
#define logInterrupcion(fmt, ...) escribirLog(LOG_INTERRUPCION, fmt, ##__VA_ARGS__)
#define logSistema(fmt, ...) escribirLog(LOG_SISTEMA, fmt, ##__VA_ARGS__)
#define logDebug(fmt, ...) escribirLog(LOG_DEBUG, fmt, ##__VA_ARGS__)

#endif
