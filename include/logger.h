#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

// Estes es un enumerado para categorizar los tipos de mensajes en el log
typedef enum {
    LOG_CPU,            // Mensajes del CPU (ciclos, instrucciones)
    LOG_MEMORIA,        // Mensajes de acceso a memoria
    LOG_DISCO,          // Mensajes de acceso al disco
    LOG_DMA,            // Mensajes del controlador DMA
    LOG_LOADER,         // Mensajes del cargador de programas
    LOG_INTERRUPCION,   // Mensajes de interrupciones 
    LOG_SISTEMA,        // Mensajes generales del sistema
    LOG_DEBUG           // Mensajes de paso a paso
} NivelLog;

// CONSTANTES DEL LOGGER
// Nombre del archivo de log por defecto
#define ARCHIVO_LOG_DEFECTO "maquina_virtual.log"

// Tamanio maximo de un mensaje de log
#define MAX_MENSAJE_LOG 512

// VARIABLES GLOBALES DEL LOGGER
// Puntero al archivo de log abierto
extern FILE *archivoLog;

// Indica si el logger esta activo
extern int loggerActivo;

// Contador de lineas de log
extern int contadorLineasLog;

// PROTOTIPOS DE FUNCIONES DEL LOGGER   

// Cabecera de la funcio para iniciar el log
int inicializarLogger(const char *nombreArchivo);

// Cabecera de la funcion para cerra el log
void finalizarLogger();

// Cabecera de la funcion que escribe mensajes en el log
void escribirLog(NivelLog nivel, const char *formato, ...);

//Una Macro es como un "atajo" o una función de "buscar y reemplazar" inteligente.

/* Se le dice al compilador "Cada vez que veas algo que diga logCpu(...), cámbialo automáticamente por 
escribirLog(LOG_CPU, ...)
*/

// Macro para log de CPU (mas conveniente de usar)
#define logCpu(fmt, ...) escribirLog(LOG_CPU, fmt, ##__VA_ARGS__)

// Macro para log de memoria
#define logMemoria(fmt, ...) escribirLog(LOG_MEMORIA, fmt, ##__VA_ARGS__)

// Macro para log de disco
#define logDisco(fmt, ...) escribirLog(LOG_DISCO, fmt, ##__VA_ARGS__)

// Macro para log de DMA
#define logDma(fmt, ...) escribirLog(LOG_DMA, fmt, ##__VA_ARGS__)

// Macro para log del loader
#define logLoader(fmt, ...) escribirLog(LOG_LOADER, fmt, ##__VA_ARGS__)

// Macro para log de interrupciones
#define logInterrupcion(fmt, ...) escribirLog(LOG_INTERRUPCION, fmt, ##__VA_ARGS__)

// Macro para log del sistema
#define logSistema(fmt, ...) escribirLog(LOG_SISTEMA, fmt, ##__VA_ARGS__)

// Macro para log de depuracion
#define logDebug(fmt, ...) escribirLog(LOG_DEBUG, fmt, ##__VA_ARGS__)

#endif
