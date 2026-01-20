#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/loader.h"
#include "../include/memoria.h"
#include "../include/hardware.h"
#include "../include/logger.h"
#include "../include/cpu.h"

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
    logLoader("Loader inicializado. Direccion base: %d", 
           siguienteDireccionDisponible);
}

int cargarPrograma(const char *rutaArchivo, int direccionDestino) {
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
    archivo = fopen(rutaArchivo, "r");
    if (archivo == NULL) {
        logLoader("ERROR: No se pudo abrir el archivo %s", rutaArchivo);
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

        // Detectar fin del programa (una linea con solo '.')
        if (linea[0] == '.' && strlen(linea) == 1) {
            logLoader("Fin del programa detectado");
            break;
        }

        // Si llegamos aqui, esperamos una instruccion: cadena de digitos (hasta 8)
        int len = strlen(linea), i, ok = 1;
        for (i = 0; i < len; i++) {
            if (linea[i] < '0' || linea[i] > '9') { ok = 0; break; }
        }
        if (!ok || len == 0 || len > 8) {
            logLoader("ERROR: Instruccion invalida en archivo: '%s'", linea);
            goto cleanup;
        }

        // Convertir la linea a entero largo
        valorInstruccion = atol(linea);
        if (valorInstruccion < 0 || valorInstruccion > 99999999L) {
            logLoader("ERROR: Valor de instruccion fuera de rango: %ld", valorInstruccion);
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
                logLoader("ERROR: No hay memoria para buffer");
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
        logLoader("ERROR: No se encontraron instrucciones");
        goto cleanup;
    }

    // Determinar direccion base
    int direccionBase;
    int modoManual = (direccionDestino != -1);

    if (modoManual) {
        direccionBase = direccionDestino;
        if (direccionBase < INICIO_MEMORIA_USUARIO) {
            logLoader("ERROR: Intento de cargar en zona del SO (0-%d). Direccion solicitada: %d", 
                      INICIO_MEMORIA_USUARIO - 1, direccionBase);
            printf("[LOADER] ERROR: Zona reservada para el Sistema Operativo.\n");
            goto cleanup;
        }
    } else {
        direccionBase = siguienteDireccionDisponible;
    }

    // Verificar espacio en memoria
    if (direccionBase + bufferLen >= TAMANO_MEMORIA) {
        logLoader("ERROR: Memoria insuficiente. Fin de programa (%d) excede memoria (%d)", 
                  direccionBase + bufferLen, TAMANO_MEMORIA);
        printf("[LOADER] ERROR: El programa no cabe en la memoria restante.\n");
        goto cleanup;
    }

    // VERIFICACION DE COLISIONES
    // Verificar si el rango de memoria objetivo ya tiene contenido (distinto de 0)
    int direccionFin = direccionBase + bufferLen;
    // RESERVA DE PILA: Se prohibe cargar en las ultimas 50 posiciones
    // para garantizar espacio minimo para la pila del sistema.
    if (direccionFin > TAMANO_MEMORIA - 50) { 
        logLoader("ERROR: Intento de cargar en zona de PILA (1950-1999). Fin del programa: %d", direccionFin);
        printf("[LOADER] ERROR: No hay espacio seguro. Las ultimas 50 posiciones estan RESERVADAS para la Pila.\n");
        goto cleanup;
    }

    // Verificar si hay datos preexistentes en el rango
    for (int k = 0; k < bufferLen; k++) {
        Palabra p = leerMemoria(direccionBase + k);
        if (p.digitos != 0 || p.signo != 0) {
            logLoader("ERROR: Memoria ocupada en direcccon %d. No se puede cargar.", direccionBase + k);
            printf("[LOADER] ERROR: Conflicto de memoria en direccion %d. Ya contiene datos.\n", direccionBase + k);
            goto cleanup;
        }
    }
    if (numeroPalabrasHeader != -1 && numeroPalabrasHeader != bufferLen) {
        logLoader("ERROR: .NumeroPalabras (%d) no coincide con instrucciones leidas (%d)",
               numeroPalabrasHeader, bufferLen);
        goto cleanup;
    }
    // Validar _start (base 1, de 1 a bufferLen)
    if (lineaInicio < 1 || lineaInicio > bufferLen) {
        logLoader("ERROR: _start invalido o fuera de rango (debe ser 1..%d)", bufferLen);
        goto cleanup;
    }

    // Escribir buffer a memoria (commit)
    int i;
    // direccionBase ya fue calculada arriba
    for (i = 0; i < bufferLen; i++) {
        escribirMemoria(direccionBase + i, buffer[i]);
        logLoader("Instruccion %d cargada en direccion %d: %d%07d",
               i, direccionBase + i, buffer[i].signo, buffer[i].digitos);
    }

    // Guardar informacion del programa
    strncpy(programaActual.nombre, nombrePrograma, MAX_NOMBRE_PROGRAMA - 1);
    programaActual.nombre[MAX_NOMBRE_PROGRAMA - 1] = '\0';
    programaActual.lineaInicio = lineaInicio - 1;  // Convertir a base 0
    programaActual.numeroPalabras = bufferLen;
    programaActual.direccionBase = direccionBase;
    programaActual.direccionLimite = direccionBase + bufferLen - 1;  // Ultima instruccion

    logLoader("Programa '%s' cargado exitosamente", programaActual.nombre);
    logLoader("Instrucciones: %d, RB: %d, RL: %d, PC inicial: %d",
           bufferLen, programaActual.direccionBase, programaActual.direccionLimite, programaActual.lineaInicio);

    // Solo actualizar siguienteDireccionDisponible si estamos en modo automatico
    if (!modoManual) {
        siguienteDireccionDisponible = direccionBase + bufferLen;
    } else {
        // En modo manual, si cargamos "mas alla", podriamos actualizarla tambien para evitar huecos,
        // pero mejor dejarlo intacto o moverlo al final de lo nuevo si es mayor.
        // Por simplicidad, si es manual, no movemos el puntero automatico a menos que lo supere.
        if (direccionBase + bufferLen > siguienteDireccionDisponible) {
            siguienteDireccionDisponible = direccionBase + bufferLen;
        }
    }
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

    // LIMPIEZA DE ESTADO DE INTERRUPCIONES
    // Es fundamental limpiar cualquier interrupcion pendiente de ejecuciones anteriores
    // (especialmente si terminaron por error fatal), de lo contrario se dispararian
    // en el primer ciclo del nuevo programa.
    interrupcionPendiente = 0;
    codigoInterrupcionPendiente = -1;

    logLoader("CPU preparado para ejecucion:");
    logLoader("  RB=%d, RL=%d, PC=%d (logico)",
           registrosCpu.rb, registrosCpu.rl, registrosCpu.psw.pc);
    logLoader("  RX=%d, SP=%d",
           registrosCpu.rx, registrosCpu.sp);
}
