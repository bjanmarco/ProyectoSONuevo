#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/loader.h"
#include "../include/memoria.h"
#include "../include/hardware.h"
#include "../include/logger.h"

// Siguiente direccion de memoria disponible para cargar programas
int siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO;

// Informacion del programa actualmente cargado (esta es la estrutura de la info)
InfoPrograma programaActual;

// Referencia externa a los registros del CPU (definidos en cpu.c)
extern Registros registrosCpu;

// Funcion externa para reiniciar deteccion de bucles (definida en cpu.c)
extern void reiniciarDeteccionBucle();

static void limpiarProgramaActual() {
    // memeset es para llenar la memoria con 0 y asi borras lo que tenia antes  
    memset(programaActual.nombre, 0, MAX_NOMBRE_PROGRAMA);
    programaActual.lineaInicio = 0;
    programaActual.numeroPalabras = 0;
    programaActual.direccionBase = 0;
    programaActual.direccionLimite = 0;
    //y el struct queda listo para guarda la nueva info del siguiente programa
}

void inicializarLoader() {
    siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO; // partimos de la base del usuario
    limpiarProgramaActual();
    logLoader("Loader inicializado. Direccion base: %d", 
    // usamos el Macro El atajo para imprimir el valor de la siguiente direccion disponible
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

    logLoader("Intentando cargar: %s", rutaArchivo);

    // Abrir el archivo
    archivo = fopen(rutaArchivo, "r"); // abrimos leyendo
    if (archivo == NULL) {
        logLoader("ERROR: No se pudo abrir el archivo %s", rutaArchivo);
        return 1;
    }

    // Leer y validar el archivo con un ile par ahcerlo linea por linea 
    while (fgets(linea, MAX_LINEA, archivo) != NULL) {
        // fgets incluye el salto por eso luego se elimina
        linea[strcspn(linea, "\n")] = '\0';  // Eliminar salto de linea y lo cambia por un fin de cadena \0

        // Ignorar lineas vacias y comentarios
        if (strlen(linea) == 0 || linea[0] == '/' || linea[0] == '#') continue;

        // Parsear _start
        // y bueno strncmp sirve para comparar strings le dices con que quiere comparar y hasta onde debe comparar
        // si es igual entra al if, y si no error
        if (strncmp(linea, "_start", 6) == 0) {
            // sscanf lee datos de una linea y los compara con lo que le pides
            if (sscanf(linea, "_start %d", &lineaInicio) != 1) {
                logLoader("ERROR: _start invalido");
                goto cleanup;
            }
            logLoader("_start = %d", lineaInicio);
            continue;
        }

        // Parsear .NumeroPalabras
        if (strncmp(linea, ".NumeroPalabras", 15) == 0) {
            if (sscanf(linea, ".NumeroPalabras %d", &numeroPalabrasHeader) != 1) {
                logLoader("ERROR: .NumeroPalabras invalido");
                goto cleanup;
            }
            logLoader("NumeroPalabras (encabezado) = %d", numeroPalabrasHeader);
            continue;
        }

        // Parsear .NombreProg
        if (strncmp(linea, ".NombreProg", 11) == 0) {
            if (sscanf(linea, ".NombreProg %49s", nombrePrograma) != 1) {
                logLoader("ERROR: .NombreProg invalido");
                goto cleanup;
            }
            logLoader("NombreProg = %s", nombrePrograma);
            continue;
        }

        // Detectar fin del programa 
        if (linea[0] == '.' && strlen(linea) == 1) {
            logLoader("Fin del programa detectado");
            break;
        }

        // Si llegamos aqui, esperamos una instruccion
        int len = strlen(linea), i, ok = 1;
        for (i = 0; i < len; i++) {
            if (linea[i] < '0' || linea[i] > '9') { ok = 0; break; } 
            // porque las ir son de 8 digitos
        }
        if (!ok || len == 0 || len > 8) {
            logLoader("ERROR: Instruccion invalida en archivo: '%s'", linea);
            goto cleanup;
        }

        // Convertir la linea a entero largo
        valorInstruccion = atol(linea); // pasa string a long
        if (valorInstruccion < 0 || valorInstruccion > 99999999L) {
            logLoader("ERROR: Valor de instruccion fuera de rango: %ld", valorInstruccion);
            goto cleanup;
        }

        // Las instrucciones son positivas en este formato 
        instruccion.signo = 0;
        instruccion.digitos = (int)valorInstruccion;

        // Guardamos las ir del programa temporalmente en un buffer 
        if (bufferLen >= bufferCap) { // el len son las que llevamos y el cap las totales
            int nuevaCap = (bufferCap == 0) ? 16 : bufferCap * 2;
            // si esta vacia le damos 16 y si no el doble
            Palabra *tmp = (Palabra*)realloc(buffer, nuevaCap * sizeof(Palabra));
            if (tmp == NULL) {
                logLoader("ERROR: No hay memoria para buffer");
                goto cleanup; // si no hay memoria para el buffer, salimos
            }
            buffer = tmp; bufferCap = nuevaCap; // actualizamos
        }
        buffer[bufferLen++] = instruccion; // guardamosy aumentamos el len
    }

    fclose(archivo);
    archivo = NULL;  // Marcar como cerrado

    // Verificar que se leyeron instrucciones
    if (bufferLen == 0) {
        logLoader("ERROR: No se encontraron instrucciones");
        goto cleanup; // salto al final para cerra el archivo
    }
    // Verificar que NumeroPalabras coincida 
    if (numeroPalabrasHeader != -1 && numeroPalabrasHeader != bufferLen) {
        logLoader("ERROR: .NumeroPalabras (%d) no coincide con instrucciones leidas (%d)",
               numeroPalabrasHeader, bufferLen);
        goto cleanup;
    }
    // Validar _start 
    if (lineaInicio < 1 || lineaInicio > bufferLen) {
        logLoader("ERROR: _start invalido o fuera de rango (debe ser 1..%d)", bufferLen);
        goto cleanup;
    }
    // Verificar espacio en memoria
    if (siguienteDireccionDisponible + bufferLen >= TAMANO_MEMORIA) {
        logLoader("ERROR: Memoria insuficiente para cargar el programa");
        goto cleanup;
    }

    // Escribir buffer a memoria
    int i, direccionBase = siguienteDireccionDisponible;
    for (i = 0; i < bufferLen; i++) {
        escribirMemoria(direccionBase + i, buffer[i]);
        logLoader("Instruccion %d cargada en direccion %d: %d%07d",
               i, direccionBase + i, buffer[i].signo, buffer[i].digitos);
    }

    // Todo lo que se obtuvo despues e leer y validar se guarda en la estructura
    strncpy(programaActual.nombre, nombrePrograma, MAX_NOMBRE_PROGRAMA - 1);
    programaActual.nombre[MAX_NOMBRE_PROGRAMA - 1] = '\0';
    programaActual.lineaInicio = lineaInicio - 1;  // Convertir a base 0
    programaActual.numeroPalabras = bufferLen;
    programaActual.direccionBase = direccionBase;
    programaActual.direccionLimite = direccionBase + bufferLen - 1;  // Ultima instruccion

    logLoader("Programa '%s' cargado exitosamente", programaActual.nombre);
    logLoader("Instrucciones: %d, RB: %d, RL: %d, PC inicial: %d",
           bufferLen, programaActual.direccionBase, programaActual.direccionLimite, programaActual.lineaInicio);

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
    registrosCpu.rl = programaActual.direccionLimite;


    // Establecer PC en la linea de inicio
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

    logLoader("CPU preparado para ejecucion:");
    logLoader("  RB=%d, RL=%d, PC=%d (logico)",
           registrosCpu.rb, registrosCpu.rl, registrosCpu.psw.pc);
    logLoader("  RX=%d, SP=%d",
           registrosCpu.rx, registrosCpu.sp);
}

// Reinicia el loader
void reiniciarLoader() {
    siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO;
    limpiarProgramaActual();
    logLoader("Loader reiniciado. Direccion base: %d", siguienteDireccionDisponible);
}
