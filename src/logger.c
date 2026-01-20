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

// Esta funcion es solo para poder asignarle el nivel a su impresion correspondiente, usamos un switch sencillo
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

// Esta funcion es solo para poder obtener el tiempo actual
static void obtenerTimestamp(char *buffer, int tamanio) {
    time_t ahora; // se guarda la hora en formato de segundos
    struct tm *tiempoLocal; // se guarda la hora en formato local
    
    time(&ahora); // se obtiene la hora actual
    tiempoLocal = localtime(&ahora); // se obtiene la hora local
    
    strftime(buffer, tamanio, "%H:%M:%S", tiempoLocal); // se obtiene la hora en formato de string 
    // strftime es como un printf pero para tiempo
}

// Esta funcion es solo para poder inicializar el logger
int inicializarLogger(const char *nombreArchivo) {
    const char *archivo; // nombre del archivo
    char timestamp[20]; // guarda la hora actual
    
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
    fflush(archivoLog); // con esta funcion se asegura que se escriba inmediatamente
    printf("[LOGGER] Sistema de log inicializado: %s\n", archivo);
    return 0;
}

void finalizarLogger() {
    char timestamp[20]; // guarda la hora actual
    
    if (archivoLog != NULL && loggerActivo) {
        obtenerTimestamp(timestamp, sizeof(timestamp));
        
        // Escribir pie del log
        fprintf(archivoLog, " FIN DEL LOG\n");
        fprintf(archivoLog, " Finalizado: %s\n", timestamp);
        fprintf(archivoLog, " Total lineas: %d\n", contadorLineasLog);
        
        fclose(archivoLog);
        archivoLog = NULL;
        loggerActivo = 0;
        
        printf("[LOGGER] Sistema de log finalizado. Total lineas: %d\n", contadorLineasLog);
    }
}

void escribirLog(NivelLog nivel, const char *formato, ...) {
    char timestamp[20]; // guarda la hora actual
    char mensaje[MAX_MENSAJE_LOG]; // guarda el mensaje
    va_list args; // lista de argumentos
    
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
    
    // Obtener la hora actual
    obtenerTimestamp(timestamp, sizeof(timestamp));
    
    // Formatear el mensaje
    va_start(args, formato); // lista de args variables
    vsnprintf(mensaje, MAX_MENSAJE_LOG, formato, args); // es como un print pero del arreglo mensaje
    va_end(args); // finaliza la lista de args variables
    
    // Escribir en archivo de log
    fprintf(archivoLog, "[%s][%s] %s\n", 
            timestamp, 
            obtenerNombreNivel(nivel), 
            mensaje);
    fflush(archivoLog);  
    
    contadorLineasLog++;
    
    // Las interrupciones tambien van a terminal
    if (nivel == LOG_INTERRUPCION) {
        printf("[%s][%s] %s\n", timestamp, obtenerNombreNivel(nivel), mensaje);
    }
}
