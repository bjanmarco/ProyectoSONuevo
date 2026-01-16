/*
 * ============================================================================
 * ARCHIVO: loader.c
 * DESCRIPCION: Implementacion del cargador de programas.
 *              Lee archivos de programa y los carga en memoria.
 *              Segun especificaciones de prueba.txt seccion 6.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/loader.h"
#include "../include/memoria.h"
#include "../include/hardware.h"

/* ============================================================================
 * DEFINICION DE VARIABLES GLOBALES
 * ============================================================================ */

// Siguiente direccion de memoria disponible para cargar programas
// Inicia en 300 (INICIO_MEMORIA_USUARIO)
int siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO;

// Informacion del programa actualmente cargado
InfoPrograma programaActual;

// Referencia externa a los registros del CPU (definidos en cpu.c)
extern Registros registrosCpu;

/* ============================================================================
 * IMPLEMENTACION DE FUNCIONES
 * ============================================================================ */

/*
 * inicializarLoader
 * -----------------
 * Inicializa el loader al estado inicial.
 */
void inicializarLoader() {
    // Establecer la primera direccion disponible
    siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO;  // 300
    
    // Limpiar informacion del programa actual
    memset(programaActual.nombre, 0, MAX_NOMBRE_PROGRAMA);
    programaActual.lineaInicio = 0;
    programaActual.numeroPalabras = 0;
    programaActual.direccionBase = 0;
    programaActual.direccionLimite = 0;
    
    printf("[LOADER] Loader inicializado. Direccion base: %d\n", 
           siguienteDireccionDisponible);
}

/*
 * cargarPrograma
 * ---------------
 * Carga un programa desde archivo a memoria.
 */
int cargarPrograma(const char *rutaArchivo) {
    FILE *archivo;
    char linea[MAX_LINEA];
    int lineaInicio = -1;
    int numeroPalabrasHeader = -1;
    char nombrePrograma[MAX_NOMBRE_PROGRAMA] = "";
    int instruccionesLeidas = 0;
    Palabra instruccion;
    long valorInstruccion;

    // Buffer temporal para validar antes de escribir a memoria
    Palabra *buffer = NULL;
    int bufferCap = 0;
    int bufferLen = 0;

    printf("[LOADER] Intentando cargar: %s\n", rutaArchivo);

    // Abrir el archivo
    archivo = fopen(rutaArchivo, "r");
    if (archivo == NULL) {
        // Intentar rutas alternativas: basename (sin directorio) y ../rutaArchivo
        const char *base = rutaArchivo;
        char rutaAlt[512];
        const char *slash = strrchr(rutaArchivo, '/');
        if (slash != NULL) base = slash + 1;

        archivo = fopen(base, "r");
        if (archivo == NULL) {
            snprintf(rutaAlt, sizeof(rutaAlt), "../%s", rutaArchivo);
            archivo = fopen(rutaAlt, "r");
        }
        if (archivo == NULL) {
            printf("[LOADER] ERROR: No se pudo abrir el archivo %s\n", rutaArchivo);
            return 1;
        } else {
            printf("[LOADER] Abierto archivo alternativo: %s\n", (slash != NULL) ? base : rutaAlt);
        }
    }

    // Leer y validar el archivo (no escribir en memoria aún)
    while (fgets(linea, MAX_LINEA, archivo) != NULL) {
        // Eliminar salto de linea
        linea[strcspn(linea, "\n")] = '\0';

        // Ignorar lineas vacias y comentarios
        if (strlen(linea) == 0 || linea[0] == '/' || linea[0] == '#') {
            continue;
        }

        // Parsear _start
        if (strncmp(linea, "_start", 6) == 0) {
            if (sscanf(linea, "_start %d", &lineaInicio) != 1) {
                printf("[LOADER] ERROR: _start invalido\n");
                fclose(archivo);
                return 1;
            }
            printf("[LOADER] _start = %d\n", lineaInicio);
            continue;
        }

        // Parsear .NumeroPalabras
        if (strncmp(linea, ".NumeroPalabras", 15) == 0) {
            if (sscanf(linea, ".NumeroPalabras %d", &numeroPalabrasHeader) != 1) {
                printf("[LOADER] ERROR: .NumeroPalabras invalido\n");
                fclose(archivo);
                return 1;
            }
            printf("[LOADER] NumeroPalabras (encabezado) = %d\n", numeroPalabrasHeader);
            continue;
        }

        // Parsear .NombreProg
        if (strncmp(linea, ".NombreProg", 11) == 0) {
            if (sscanf(linea, ".NombreProg %49s", nombrePrograma) != 1) {
                printf("[LOADER] ERROR: .NombreProg invalido\n");
                fclose(archivo);
                return 1;
            }
            printf("[LOADER] NombreProg = %s\n", nombrePrograma);
            continue;
        }

        // Detectar fin del programa (una linea con solo '.')
        if (linea[0] == '.' && strlen(linea) == 1) {
            printf("[LOADER] Fin del programa detectado\n");
            break;
        }

        // Si llegamos aqui, esperamos una instruccion: cadena de digitos (hasta 8)
        int len = 0;
        while (linea[len] != '\0') len++;
        // Validar que la linea contenga solo digitos (0-9)
        int i, ok = 1;
        for (i = 0; i < len; i++) {
            if (linea[i] < '0' || linea[i] > '9') { ok = 0; break; }
        }
        if (!ok || len == 0 || len > 8) {
            printf("[LOADER] ERROR: Instruccion invalida en archivo: '%s'\n", linea);
            fclose(archivo);
            free(buffer);
            return 1;
        }

        // Convertir la linea a entero largo
        valorInstruccion = atol(linea);
        if (valorInstruccion < 0 || valorInstruccion > 99999999L) {
            printf("[LOADER] ERROR: Valor de instruccion fuera de rango: %ld\n", valorInstruccion);
            fclose(archivo);
            free(buffer);
            return 1;
        }

        // Convertir a Palabra: mantener el valor completo en 'digitos'
        // (las instrucciones se representan como valores positivos de 8 digitos)
        instruccion.signo = 0;
        instruccion.digitos = (int)valorInstruccion;

        // Agregar al buffer dinamico
        if (bufferLen >= bufferCap) {
            int nuevaCap = (bufferCap == 0) ? 16 : bufferCap * 2;
            Palabra *tmp = (Palabra*)realloc(buffer, nuevaCap * sizeof(Palabra));
            if (tmp == NULL) {
                printf("[LOADER] ERROR: No hay memoria para buffer\n");
                fclose(archivo);
                free(buffer);
                return 1;
            }
            buffer = tmp; bufferCap = nuevaCap;
        }
        buffer[bufferLen++] = instruccion;
    }

    fclose(archivo);

    // Verificar que se leyeron instrucciones
    if (bufferLen == 0) {
        printf("[LOADER] ERROR: No se encontraron instrucciones\n");
        free(buffer);
        return 1;
    }

    // Si el encabezado especifica NumeroPalabras, verificar que coincida
    if (numeroPalabrasHeader != -1 && numeroPalabrasHeader != bufferLen) {
        printf("[LOADER] ERROR: .NumeroPalabras (%d) no coincide con instrucciones leidas (%d)\n",
               numeroPalabrasHeader, bufferLen);
        free(buffer);
        return 1;
    }

    // Validar _start
    if (lineaInicio < 0 || lineaInicio >= bufferLen) {
        printf("[LOADER] ERROR: _start invalido o fuera de rango\n");
        free(buffer);
        return 1;
    }

    // Verificar espacio en memoria antes de escribir
    if (siguienteDireccionDisponible + bufferLen >= TAMANO_MEMORIA) {
        printf("[LOADER] ERROR: Memoria insuficiente para cargar el programa\n");
        free(buffer);
        return 1;
    }

    // Escribir buffer a memoria (commit)
    int i;
    int direccionBase = siguienteDireccionDisponible;
    for (i = 0; i < bufferLen; i++) {
        escribirMemoria(direccionBase + i, buffer[i]);
        printf("[LOADER] Instruccion %d cargada en direccion %d: %d%07d\n",
               i, direccionBase + i, buffer[i].signo, buffer[i].digitos);
    }

    // Nota: El uso de centinela fue eliminado; no escribimos centinela.

    // Guardar informacion del programa
    strncpy(programaActual.nombre, nombrePrograma, MAX_NOMBRE_PROGRAMA - 1);
    programaActual.nombre[MAX_NOMBRE_PROGRAMA - 1] = '\0';
    programaActual.lineaInicio = lineaInicio;
    programaActual.numeroPalabras = bufferLen;
    programaActual.direccionBase = direccionBase;
    // RL = ultima direccion valida (base + numeroPalabras - 1)
    programaActual.direccionLimite = direccionBase + bufferLen - 1;

    printf("[LOADER] ============================================\n");
    printf("[LOADER] Programa '%s' cargado exitosamente\n", programaActual.nombre);
    printf("[LOADER] Instrucciones: %d\n", bufferLen);
    printf("[LOADER] RB (direccion base): %d\n", programaActual.direccionBase);
    printf("[LOADER] RL (direccion limite): %d\n", programaActual.direccionLimite);
    printf("[LOADER] PC inicial (logico): %d\n", lineaInicio);
    printf("[LOADER] ============================================\n");

    // Actualizar siguiente direccion disponible (despues del programa cargado)
    siguienteDireccionDisponible = direccionBase + bufferLen;

    free(buffer);
    return 0;  // Exito
} 

/*
 * prepararEjecucion
 * -----------------
 * Configura los registros del CPU para ejecutar el programa cargado.
 */
void prepararEjecucion() {
    // Establecer registros de proteccion
    registrosCpu.rb = programaActual.direccionBase;
    registrosCpu.rl = programaActual.direccionLimite; // RL = ultima direccion valida

    // Establecer PC en la linea de inicio (direccion logica)
    registrosCpu.psw.pc = programaActual.lineaInicio;

    // Establecer pila en la cima de la memoria física (base fija 1999)
    registrosCpu.rx = 1999;
    registrosCpu.sp = 1999;

    // Modo usuario con interrupciones habilitadas
    /* Por defecto ejecutar en modo usuario. Si el nombre del programa contiene
     * la palabra "kernel" se ejecutará en modo kernel (para pruebas). */
    if (strstr(programaActual.nombre, "kernel") != NULL) {
        registrosCpu.psw.modoOperacion = MODO_KERNEL;
    } else {
        registrosCpu.psw.modoOperacion = MODO_USUARIO;
    }
    registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
    registrosCpu.psw.codigoCondicion = CC_CERO;

    // Limpiar acumulador
    registrosCpu.ac.signo = 0;
    registrosCpu.ac.digitos = 0;

    printf("[LOADER] CPU preparado para ejecucion:\n");
    printf("[LOADER]   RB=%d, RL=%d, PC=%d (logico)\n",
           registrosCpu.rb, registrosCpu.rl, registrosCpu.psw.pc);
    printf("[LOADER]   RX=%d, SP=%d\n",
           registrosCpu.rx, registrosCpu.sp);
} 

/*
 * reiniciarLoader
 * ---------------
 * Reinicia el loader para comenzar desde el inicio.
 */
void reiniciarLoader() {
    siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO;
    
    memset(programaActual.nombre, 0, MAX_NOMBRE_PROGRAMA);
    programaActual.lineaInicio = 0;
    programaActual.numeroPalabras = 0;
    programaActual.direccionBase = 0;
    programaActual.direccionLimite = 0;
    
    printf("[LOADER] Loader reiniciado. Direccion base: %d\n",
           siguienteDireccionDisponible);
}
