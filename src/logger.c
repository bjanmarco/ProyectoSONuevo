#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "../include/logger.h"

// Puntero al archivo de log
FILE *archivoLog = NULL;

// Indica si el logger esta activo
int loggerActivo = 0;

// Contador de lineas de log
int contadorLineasLog = 0;


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
    
    // Usar nombre por defecto si no se proporciona
    if (nombreArchivo == NULL || strlen(nombreArchivo) == 0) {
        archivo = ARCHIVO_LOG_DEFECTO;
    } else {
        archivo = nombreArchivo;
    }
    
    // Abrir archivo en modo escritura (sobreescribe si existe)
    archivoLog = fopen(archivo, "w");
    if (archivoLog == NULL) {
        printf("[LOGGER] ERROR: No se pudo abrir archivo de log: %s\n", archivo);
        return 1;
    }
    
    // Marcar logger como activo
    loggerActivo = 1;
    contadorLineasLog = 0;
    
    // Escribir cabecera del log
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
        
        // Escribir pie del log
        fprintf(archivoLog, " FIN DEL LOG\n");
        fprintf(archivoLog, " Finalizado: %s\n", timestamp);
        fprintf(archivoLog, " Total lineas: %d\n", contadorLineasLog);
        
        fclose(archivoLog);
        archivoLog = NULL;
        loggerActivo = 0;
        
        printf("[LOGGER] Log finalizado. Total lineas: %d\n", contadorLineasLog);
    }
}

void escribirLog(NivelLog nivel, const char *formato, ...) {
    char timestamp[20];
    char mensaje[MAX_MENSAJE_LOG];
    va_list args;
    
    // Si el logger no esta activo, solo imprimir en consola
    if (!loggerActivo || archivoLog == NULL) {
        // Para interrupciones, siempre imprimir en stdout
        if (nivel == LOG_INTERRUPCION) {
            va_start(args, formato);
            printf("[%s] ", obtenerNombreNivel(nivel));
            vprintf(formato, args);
            printf("\n");
            va_end(args);
        }
        return;
    }
    
    // Obtener timestamp
    obtenerTimestamp(timestamp, sizeof(timestamp));
    
    // Formatear el mensaje
    va_start(args, formato);
    vsnprintf(mensaje, MAX_MENSAJE_LOG, formato, args);
    va_end(args);
    
    // Escribir en archivo de log
    fprintf(archivoLog, "[%s][%s] %s\n", 
            timestamp, 
            obtenerNombreNivel(nivel), 
            mensaje);
    fflush(archivoLog);  // Asegurar que se escriba inmediatamente
    
    contadorLineasLog++;
    
    // Las interrupciones tambien van a stdout (segun especificacion)
    if (nivel == LOG_INTERRUPCION) {
        printf("[%s][%s] %s\n", timestamp, obtenerNombreNivel(nivel), mensaje);
    }
}
