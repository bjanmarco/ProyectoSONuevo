#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/loader.h"
#include "../include/memoria.h"
#include "../include/hardware.h"

// Siguiente direccion de memoria disponible para cargar programas
// Inicia en 300 (INICIO_MEMORIA_USUARIO)
int siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO;

// Informacion del programa actualmente cargado
InfoPrograma programaActual;

// Referencia externa a los registros del CPU (definidos en cpu.c)
extern Registros registrosCpu;

// Funcion externa para reiniciar deteccion de bucles (definida en cpu.c)
extern void reiniciarDeteccionBucle();

static void limpiarProgramaActual() {
    memset(programaActual.nombre, 0, MAX_NOMBRE_PROGRAMA);
    programaActual.lineaInicio = 0;
    programaActual.numeroPalabras = 0;
    programaActual.direccionBase = 0;
    programaActual.direccionLimite = 0;
}

void inicializarLoader() {
    siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO;
    limpiarProgramaActual();
    printf("[LOADER] Loader inicializado. Direccion base: %d\n", 
           siguienteDireccionDisponible);
}

int cargarPrograma(const char *rutaArchivo) {
    FILE *archivo = NULL;
    char linea[MAX_LINEA];
    int lineaInicio = -1, numeroPalabrasHeader = -1;
    char nombrePrograma[MAX_NOMBRE_PROGRAMA] = "";
    Palabra instruccion;
    long valorInstruccion;
    int resultado = 1;  // Por defecto error, cambia a 0 si todo sale bien

    // Buffer temporal para validar antes de escribir a memoria
    Palabra *buffer = NULL;
    int bufferCap = 0, bufferLen = 0;

    printf("[LOADER] Intentando cargar: %s\n", rutaArchivo);

    // Abrir el archivo
    archivo = fopen(rutaArchivo, "r");
    if (archivo == NULL) {
        printf("[LOADER] ERROR: No se pudo abrir el archivo %s\n", rutaArchivo);
        return 1;
    }

    // Leer y validar el archivo (no escribir en memoria aún)
    while (fgets(linea, MAX_LINEA, archivo) != NULL) {
        linea[strcspn(linea, "\n")] = '\0';  // Eliminar salto de linea

        // Ignorar lineas vacias y comentarios
        if (strlen(linea) == 0 || linea[0] == '/' || linea[0] == '#') continue;

        // Parsear _start
        if (strncmp(linea, "_start", 6) == 0) {
            if (sscanf(linea, "_start %d", &lineaInicio) != 1) {
                printf("[LOADER] ERROR: _start invalido\n");
                goto cleanup;
            }
            printf("[LOADER] _start = %d\n", lineaInicio);
            continue;
        }

        // Parsear .NumeroPalabras
        if (strncmp(linea, ".NumeroPalabras", 15) == 0) {
            if (sscanf(linea, ".NumeroPalabras %d", &numeroPalabrasHeader) != 1) {
                printf("[LOADER] ERROR: .NumeroPalabras invalido\n");
                goto cleanup;
            }
            printf("[LOADER] NumeroPalabras (encabezado) = %d\n", numeroPalabrasHeader);
            continue;
        }

        // Parsear .NombreProg
        if (strncmp(linea, ".NombreProg", 11) == 0) {
            if (sscanf(linea, ".NombreProg %49s", nombrePrograma) != 1) {
                printf("[LOADER] ERROR: .NombreProg invalido\n");
                goto cleanup;
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
        int len = strlen(linea), i, ok = 1;
        for (i = 0; i < len; i++) {
            if (linea[i] < '0' || linea[i] > '9') { ok = 0; break; }
        }
        if (!ok || len == 0 || len > 8) {
            printf("[LOADER] ERROR: Instruccion invalida en archivo: '%s'\n", linea);
            goto cleanup;
        }

        // Convertir la linea a entero largo
        valorInstruccion = atol(linea);
        if (valorInstruccion < 0 || valorInstruccion > 99999999L) {
            printf("[LOADER] ERROR: Valor de instruccion fuera de rango: %ld\n", valorInstruccion);
            goto cleanup;
        }

        // Las instrucciones son positivas en este formato (8 digitos)
        instruccion.signo = 0;
        instruccion.digitos = (int)valorInstruccion;

        // Agregar al buffer dinamico (expandir si es necesario)
        if (bufferLen >= bufferCap) {
            int nuevaCap = (bufferCap == 0) ? 16 : bufferCap * 2;
            Palabra *tmp = (Palabra*)realloc(buffer, nuevaCap * sizeof(Palabra));
            if (tmp == NULL) {
                printf("[LOADER] ERROR: No hay memoria para buffer\n");
                goto cleanup;
            }
            buffer = tmp; bufferCap = nuevaCap;
        }
        buffer[bufferLen++] = instruccion;
    }

    fclose(archivo);
    archivo = NULL;  // Marcar como cerrado

    // Verificar que se leyeron instrucciones
    if (bufferLen == 0) {
        printf("[LOADER] ERROR: No se encontraron instrucciones\n");
        goto cleanup;
    }
    // Verificar que NumeroPalabras coincida (si fue especificado)
    if (numeroPalabrasHeader != -1 && numeroPalabrasHeader != bufferLen) {
        printf("[LOADER] ERROR: .NumeroPalabras (%d) no coincide con instrucciones leidas (%d)\n",
               numeroPalabrasHeader, bufferLen);
        goto cleanup;
    }
    // Validar _start (base 1, de 1 a bufferLen)
    if (lineaInicio < 1 || lineaInicio > bufferLen) {
        printf("[LOADER] ERROR: _start invalido o fuera de rango (debe ser 1..%d)\n", bufferLen);
        goto cleanup;
    }
    // Verificar espacio en memoria
    if (siguienteDireccionDisponible + bufferLen >= TAMANO_MEMORIA) {
        printf("[LOADER] ERROR: Memoria insuficiente para cargar el programa\n");
        goto cleanup;
    }

    // Escribir buffer a memoria (commit)
    int i, direccionBase = siguienteDireccionDisponible;
    for (i = 0; i < bufferLen; i++) {
        escribirMemoria(direccionBase + i, buffer[i]);
        printf("[LOADER] Instruccion %d cargada en direccion %d: %d%07d\n",
               i, direccionBase + i, buffer[i].signo, buffer[i].digitos);
    }

    // Guardar informacion del programa
    strncpy(programaActual.nombre, nombrePrograma, MAX_NOMBRE_PROGRAMA - 1);
    programaActual.nombre[MAX_NOMBRE_PROGRAMA - 1] = '\0';
    programaActual.lineaInicio = lineaInicio - 1;  // Convertir a base 0
    programaActual.numeroPalabras = bufferLen;
    programaActual.direccionBase = direccionBase;
    programaActual.direccionLimite = direccionBase + bufferLen - 1;  // Ultima instruccion

    printf("[LOADER] ============================================\n");
    printf("[LOADER] Programa '%s' cargado exitosamente\n", programaActual.nombre);
    printf("[LOADER] Instrucciones: %d, RB: %d, RL: %d, PC inicial: %d\n",
           bufferLen, programaActual.direccionBase, programaActual.direccionLimite, programaActual.lineaInicio);
    printf("[LOADER] ============================================\n");

    siguienteDireccionDisponible = direccionBase + bufferLen;
    resultado = 0;  // Exito

cleanup:
    if (archivo != NULL) fclose(archivo);
    free(buffer);
    return resultado;
} 

void prepararEjecucion() {
    // Reiniciar deteccion de bucles infinitos para evitar falsos positivos
    reiniciarDeteccionBucle();

    // Establecer registros de proteccion
    registrosCpu.rb = programaActual.direccionBase;
    registrosCpu.rl = programaActual.direccionLimite; // RL = direccion de la ultima instruccion (incluido)


    // Establecer PC en la linea de inicio (direccion logica)
    // Ya se convirtio a base 0 en cargarPrograma
    registrosCpu.psw.pc = programaActual.lineaInicio;

    // Establecer pila al FINAL de la memoria
    // La pila crece hacia abajo (SP--), por lo que iniciamos en la ultima posicion
    registrosCpu.rx = TAMANO_MEMORIA - 1;
    registrosCpu.sp = TAMANO_MEMORIA - 1;

    // Modo usuario con interrupciones habilitadas
    registrosCpu.psw.modoOperacion = MODO_USUARIO;
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

void reiniciarLoader() {
    siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO;
    limpiarProgramaActual();
    printf("[LOADER] Loader reiniciado. Direccion base: %d\n", siguienteDireccionDisponible);
}
