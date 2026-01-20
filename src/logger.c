#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "../include/logger.h"

// puntero al archivo 
FILE *archivoLog = NULL;

// indica si el logger esta activo  
int loggerActivo = 0;

// esta funcion es solo para poder asignarle el nivel a su impresion correspondiente, usamos un switch sencillo
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

// para obtener el la hora actual
static void obtenerTimestamp(char *buffer, int tamanio) {
    time_t ahora; // se guarda la hora en segundos
    struct tm *tiempoLocal; // se guarda la hora en forma local
    
    time(&ahora); // se obtiene 
    tiempoLocal = localtime(&ahora); // se convierte 
    
    strftime(buffer, tamanio, "%H:%M:%S", tiempoLocal); // se obtiene la hora en formato de string 
    // strftime es como un printf pero para tiempo
}

int inicializarLogger(const char *nombreArchivo) {
    const char *archivo; // nombre del archivo
    char timestamp[20]; // hora actual
    
    // usar nombre por defecto si no se proporciona
    if (nombreArchivo == NULL || strlen(nombreArchivo) == 0) {
        archivo = ARCHIVO_LOG_DEFECTO;
    } else {
        archivo = nombreArchivo;
    }
    
    // abrir archivo en modo escritura (sobreescribe si existe)
    archivoLog = fopen(archivo, "w");
    if (archivoLog == NULL) {
        printf("[LOGGER] ERROR: No se pudo abrir archivo de log: %s\n", archivo);
        return 1;
    }
    
    // marcar logger como activo
    loggerActivo = 1;
    
    // escribir cabecera del log
    obtenerTimestamp(timestamp, sizeof(timestamp));
    fprintf(archivoLog, " Iniciado: %s\n", timestamp);
    fprintf(archivoLog, " Archivo: %s\n", archivo);
    fflush(archivoLog); // con esta funcion se asegura que se escriba inmediatamente
    // porque el logger debe ser inmediato
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
    
    // si el logger no esta activo, solo imprimir en consola
    if (!loggerActivo || archivoLog == NULL) {
        // para interrupciones, siempre imprimir en pantalla
        if (nivel == LOG_INTERRUPCION) {
            va_start(args, formato); // lista de args variables
            printf("[%s] ", obtenerNombreNivel(nivel));
            vprintf(formato, args);
            printf("\n");
            va_end(args); // finaliza la lista de args variables
        }
        return;
    }
    
    obtenerTimestamp(timestamp, sizeof(timestamp));
    
    // formatear el mensaje
    va_start(args, formato); // lista de args variables
    vsnprintf(mensaje, MAX_MENSAJE_LOG, formato, args); // es como un print pero del arreglo mensaje
    va_end(args); // finaliza la lista de args variables
    
    // escribir en archivo de log
    fprintf(archivoLog, "[%s][%s] %s\n", 
            timestamp, 
            obtenerNombreNivel(nivel), 
            mensaje);
    fflush(archivoLog);  // asegura que se escriba inmediatamente
    
    // las interrupciones tambien van en pantalla
    if (nivel == LOG_INTERRUPCION) {
        printf("[%s] %s\n", obtenerNombreNivel(nivel), mensaje);
    }
}
