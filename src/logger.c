#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "../include/logger.h"

// Puntero al archivo log
FILE *archivoLog = NULL;

// Bandera de estado  
int loggerActivo = 0;

// Identificador string para niveles
static const char* obtenerNombreNivel(NivelLog nivel) {
    switch (nivel) {
        case LOG_CPU:           return "CPU";
        case LOG_MEMORIA:       return "MEM";
        case LOG_DISCO:         return "DISCO";
        case LOG_DMA:           return "DMA";
        case LOG_LOADER:        return "LOADER";
        case LOG_INTERRUPCION:  return "INT";
        case LOG_SISTEMA:       return "SIS";
        case LOG_DEBUG:         return "DEBUG";
        default:                return "???";
    }
}

// Obtener el tiempo actual
static void obtenerTimestamp(char *buffer, int tamanio) {
    time_t ahora;
    struct tm *tiempoLocal;
    
    time(&ahora);
    tiempoLocal = localtime(&ahora);
    
    strftime(buffer, tamanio, "%H:%M:%S", tiempoLocal);
}

int inicializarLogger(const char *nombreArchivo) {
    const char *archivo;
    char timestamp[20];
    
    // Usar nombre por defecto
    if (nombreArchivo == NULL || strlen(nombreArchivo) == 0) {
        archivo = ARCHIVO_LOG_DEFECTO;
    } else {
        archivo = nombreArchivo;
    }
    
    // Abrir modo escritura
    archivoLog = fopen(archivo, "w");
    if (archivoLog == NULL) {
        printf("[LOGGER] ERROR: No se pudo abrir archivo de log: %s\n", archivo);
        return 1;
    }
    
    // Activar logger
    loggerActivo = 1;
    
    // Escribir cabecera
    obtenerTimestamp(timestamp, sizeof(timestamp));
    fprintf(archivoLog, " Iniciado: %s\n", timestamp);
    fprintf(archivoLog, " Archivo: %s\n", archivo);
    fflush(archivoLog);
    printf("\n[LOGGER] Log creado: %s\n", archivo);
    
    return 0;
}

void finalizarLogger() {
    char timestamp[20];
    
    if (archivoLog != NULL && loggerActivo) {
        obtenerTimestamp(timestamp, sizeof(timestamp));
        
        // escribir pie del log
        fprintf(archivoLog, " FIN DEL LOG\n");
        fprintf(archivoLog, " Finalizado: %s\n", timestamp);
        
        fclose(archivoLog);
        archivoLog = NULL;
        loggerActivo = 0;
        
        printf("[LOGGER] Log finalizado.\n");
    }
}

void escribirLog(NivelLog nivel, const char *formato, ...) {
    char timestamp[20];
    char mensaje[MAX_MENSAJE_LOG];
    va_list args;
    
    if (!loggerActivo || archivoLog == NULL) {
        if (nivel == LOG_INTERRUPCION) {
            va_start(args, formato);
            vsnprintf(mensaje, MAX_MENSAJE_LOG, formato, args);
            va_end(args);
            if (strstr(mensaje, "Temporizador") == NULL) {
                printf("[%s] %s\n", obtenerNombreNivel(nivel), mensaje);
            }
        }
        return;
    }
    
    obtenerTimestamp(timestamp, sizeof(timestamp));
    
    va_start(args, formato);
    vsnprintf(mensaje, MAX_MENSAJE_LOG, formato, args);
    va_end(args);
    
    fprintf(archivoLog, "[%s][%s] %s\n", 
            timestamp, 
            obtenerNombreNivel(nivel), 
            mensaje);
    fflush(archivoLog);
    
    if (nivel == LOG_INTERRUPCION && strstr(mensaje, "Temporizador") == NULL) {
        printf("[%s] %s\n", obtenerNombreNivel(nivel), mensaje);
    }
}
