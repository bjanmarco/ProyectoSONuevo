#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef enum {
    LOG_CPU,
    LOG_MEMORIA,
    LOG_DISCO,
    LOG_DMA,
    LOG_LOADER,
    LOG_INTERRUPCION,
    LOG_SISTEMA,
    LOG_DEBUG
} NivelLog;

#define ARCHIVO_LOG_DEFECTO "maquinaVirtual.log"
#define MAX_MENSAJE_LOG 512

extern FILE *archivoLog;
extern int loggerActivo;

int inicializarLogger(const char *nombreArchivo);
void finalizarLogger();
void escribirLog(NivelLog nivel, const char *formato, ...);

#define logCpu(fmt, ...) escribirLog(LOG_CPU, fmt, ##__VA_ARGS__)
#define logMemoria(fmt, ...) escribirLog(LOG_MEMORIA, fmt, ##__VA_ARGS__)
#define logDisco(fmt, ...) escribirLog(LOG_DISCO, fmt, ##__VA_ARGS__)
#define logDma(fmt, ...) escribirLog(LOG_DMA, fmt, ##__VA_ARGS__)
#define logLoader(fmt, ...) escribirLog(LOG_LOADER, fmt, ##__VA_ARGS__)
#define logInterrupcion(fmt, ...) escribirLog(LOG_INTERRUPCION, fmt, ##__VA_ARGS__)
#define logSistema(fmt, ...) escribirLog(LOG_SISTEMA, fmt, ##__VA_ARGS__)
#define logDebug(fmt, ...) escribirLog(LOG_DEBUG, fmt, ##__VA_ARGS__)

#endif
