/* ARCHIVO: logger.c
 * DESCRIPCION: Implementacion del sistema de logging centralizado.
 *              Registra todas las acciones en archivo .log y consola.
 *              Segun especificaciones de prueba.txt seccion 7.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "../include/logger.h"

// DEFINICION DE VARIABLES GLOBALES 

// Puntero al archivo de log
FILE *archivoLog = NULL;

// Indica si el logger esta activo
int loggerActivo = 0;

// Contador de lineas de log
int contadorLineasLog = 0;

// FUNCIONES AUXILIARES 

/* obtenerNombreNivel
 * Retorna el nombre del nivel de log como string.
*/
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

/* obtenerTimestamp
 * Obtiene la marca de tiempo actual en formato HH:MM:SS.
 * Esta funcion fue basada en tutoriales que explican como hacer un log basico:
*/
static void obtenerTimestamp(char *buffer, int tamanio) {
    time_t ahora;
    struct tm *tiempoLocal;
    
    time(&ahora); // mira el reloj del sistema y guarda el numero en segs
    tiempoLocal = localtime(&ahora); // convertir a hora local
    
    /*strftime funciona parecido a printf pero para horas:
    * %H = Hora (00-23)
    * %M = Minuto (00-59)
    * %S = Segundo (00-59)
    * El resultado (ej: 
    */
    strftime(buffer, tamanio, "%H:%M:%S", tiempoLocal); 
}

// IMPLEMENTACION DE FUNCIONES PUBLICAS 

/* inicializarLogger
 * Abre el archivo de log e inicializa el sistema.
*/
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
    fprintf(archivoLog, " MAQUINA VIRTUAL - LOG DE SISTEMA\n");
    fprintf(archivoLog, " Iniciado: %s\n", timestamp);
    fprintf(archivoLog, " Archivo: %s\n", archivo);
    fflush(archivoLog);
    
    printf("[LOGGER] Sistema de log inicializado: %s\n", archivo);
    
    return 0;
}

/* finalizarLogger
 * Cierra el archivo de log.
*/
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
        
        printf("[LOGGER] Sistema de log finalizado. Total lineas: %d\n", contadorLineasLog);
    }
}

/* escribirLog
 * Escribe un mensaje en el log con formato y timestamp.
*/
void escribirLog(NivelLog nivel, const char *formato, ...) {
    char timestamp[20];
    char mensaje[MAX_MENSAJE_LOG]; 
    va_list args; // lista de argumentos variables
    
    // Si el logger no esta activo, solo imprimir en consola
    if (!loggerActivo || archivoLog == NULL) {
        // Para interrupciones, siempre imprimir en stdout
        if (nivel == LOG_INTERRUPCION) {
            va_start(args, formato); // inicializa la lista de argumentos variables
            printf("[%s] ", obtenerNombreNivel(nivel)); 
            vprintf(formato, args); // printf pero para listas variables
            printf("\n");
            va_end(args);
        }
        return;
    }
    
    // Obtener timestamp
    obtenerTimestamp(timestamp, sizeof(timestamp));
    
    // Formatear el mensaje
    va_start(args, formato); // reinicia la lista de argumentos variables
    /*  vsnprintf es como vsprintf pero para cadenas
    *  Toma los argumentos variables y los formatea en una cadena, guarda el resultado en mensaje
    *  MAX_MENSAJE_LOG es el tamanio maximo de la cadena    
    */
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
