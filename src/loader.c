// este modulo lee los archivos de programa 
// y los carga en la memoria RAM para que el CPU los pueda ejecutar.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/loader.h"
#include "../include/memoria.h"
#include "../include/hardware.h"
#include "../include/logger.h"
#include "../include/cpu.h"

// siguiente direccion de memoria disponible para cargar programas
// inicia en 300 
int siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO;

// esta es el struct de la info el programa.
InfoPrograma programaActual;

// referencia externa a los registros del CPU (definidos en cpu.c)
extern Registros registrosCpu;

// funcion externa para reiniciar deteccion de bucles (definida en cpu.c)
extern void reiniciarDeteccionBucle();

static char *recortarEspacios(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    if (*s == '\0') return s;

    char *fin = s + strlen(s) - 1;
    while (fin >= s && (*fin == ' ' || *fin == '\t' || *fin == '\r')) {
        *fin = '\0';
        fin--;
    }
    return s;
}

static void quitarComentarioInline(char *s) {
    char *dobleSlash = strstr(s, "//");
    char *hash = strchr(s, '#');
    char *inicioComentario = NULL;

    if (dobleSlash != NULL) {
        inicioComentario = dobleSlash;
    }
    if (hash != NULL && (inicioComentario == NULL || hash < inicioComentario)) {
        inicioComentario = hash;
    }
    if (inicioComentario != NULL) {
        *inicioComentario = '\0';
    }
}

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

int cargarPrograma(const char *rutaArchivo, int direccionDestino) {
    FILE *archivo = NULL;
    char linea[MAX_LINEA];
    int lineaInicio = -1, numeroPalabrasHeader = -1;
    char nombrePrograma[MAX_NOMBRE_PROGRAMA] = "";
    Palabra instruccion;
    long valorInstruccion;
    int resultado = 1;  // por defecto error, cambia a 0 si todo sale bien

    // buffer temporal para validar antes de escribir a memoria
    Palabra *buffer = NULL;
    int bufferCap = 0, bufferLen = 0;

    logLoader("Intentando cargar: %s", rutaArchivo);

    // abrir el archivo
    archivo = fopen(rutaArchivo, "r"); // abrimos leyendo
    if (archivo == NULL) {
        logLoader("ERROR: No se pudo abrir el archivo %s", rutaArchivo);
        return 1;
    }

    // leer y validar el archivo con while para que sea linea por linea (no escribir en memoria aún)
    while (fgets(linea, MAX_LINEA, archivo) != NULL) {
        // fgets incluye el salto por eso luego se elimina
        linea[strcspn(linea, "\n")] = '\0';  // eliminar salto de linea y lo cambia por un fin de cadena \0

        quitarComentarioInline(linea);
        char *lineaLimpia = recortarEspacios(linea);

        // ignorar lineas vacias y comentarios
        if (strlen(lineaLimpia) == 0 || lineaLimpia[0] == '/' || lineaLimpia[0] == '#') continue;

        // parsear _start
        // y bueno strncmp sirve para comparar strings le dices con que quiere comparar y hasta onde debe comparar
        // si es igual entra al if, y si no error
        if (strncmp(lineaLimpia, "_start", 6) == 0) {
            if (sscanf(lineaLimpia, "_start %d", &lineaInicio) != 1) {
                 // sscanf lee datos de una linea y los compara con lo que le pides
                logLoader("ERROR: _start invalido");
                goto cleanup; // si hay error salgo, pero antes libero todo lo que puse (salida correcta)
            }
            logLoader("_start = %d", lineaInicio);
            continue;
        }

        // parsear .NumeroPalabras
        if (strncmp(lineaLimpia, ".NumeroPalabras", 15) == 0) {
            if (sscanf(lineaLimpia, ".NumeroPalabras %d", &numeroPalabrasHeader) != 1) {
                logLoader("ERROR: .NumeroPalabras invalido");
                goto cleanup; // salida correcta
            }
            logLoader("NumeroPalabras (encabezado) = %d", numeroPalabrasHeader);
            continue;
        }

        // parsear .NombreProg
        if (strncmp(lineaLimpia, ".NombreProg", 11) == 0) {
            if (sscanf(lineaLimpia, ".NombreProg %49s", nombrePrograma) != 1) {
                logLoader("ERROR: .NombreProg invalido");
                goto cleanup; // salida correcta
            }
            logLoader("NombreProg = %s", nombrePrograma);
            continue;
        }

        // detectar fin del programa (una linea con solo '.')
        if (lineaLimpia[0] == '.' && strlen(lineaLimpia) == 1) {
            logLoader("Fin del programa detectado");
            break;
        }

        // si llegamos aqui, esperamos una instruccion (cadena de 8 digitos)
        int len = strlen(lineaLimpia), i, ok = 1;
        for (i = 0; i < len; i++) {
            if (lineaLimpia[i] < '0' || lineaLimpia[i] > '9') { ok = 0; break; }
            // porque las ir son de 8 digitos
        }
        if (!ok || len == 0 || len > 8) {
            logLoader("ERROR: Instruccion invalida en archivo: '%s'", lineaLimpia);
            goto cleanup; // salida correcta
        }

        // convertimos la linea a entero largo
        valorInstruccion = atol(lineaLimpia); // pasa string a long
        if (valorInstruccion < 0 || valorInstruccion > 99999999L) {
            logLoader("ERROR: Valor de instruccion fuera de rango: %ld", valorInstruccion);
            goto cleanup; // salida correcta
        }

        // las instrucciones son positivas
        instruccion.signo = 0;
        instruccion.digitos = (int)valorInstruccion;

        // agregar al buffer dinamico (expandir si es necesario)
        if (bufferLen >= bufferCap) { // el len son las que llevamos y el cap las totales
            int nuevaCap = (bufferCap == 0) ? 16 : bufferCap * 2;
            // si esta vacia le damos 16 y si no el doble
            Palabra *tmp = (Palabra*)realloc(buffer, nuevaCap * sizeof(Palabra));
            if (tmp == NULL) {
                logLoader("ERROR: No hay memoria para buffer");
                goto cleanup; // salida correcta
            }
            buffer = tmp; bufferCap = nuevaCap; // actualizamos
        }
        buffer[bufferLen++] = instruccion; // guatdamos y actualizamos el len
    }

    fclose(archivo);
    archivo = NULL;  // marcar como cerrado

    // verificar que se leyeron instrucciones
    if (bufferLen == 0) {
        logLoader("ERROR: No se encontraron instrucciones");
        goto cleanup;
    }

    // determinar direccion base
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

    // verificar espacio en memoria
    if (direccionBase + bufferLen >= TAMANO_MEMORIA) {
        logLoader("ERROR: Memoria insuficiente. Fin de programa (%d) excede memoria (%d)", 
                  direccionBase + bufferLen, TAMANO_MEMORIA);
        printf("[LOADER] ERROR: El programa no cabe en la memoria restante.\n");
        goto cleanup;
    }

    // verificacion de colisiones
    // verificar si el rango de memoria objetivo ya tiene contenido (distinto de 0)
    int direccionFin = direccionBase + bufferLen;
    // reserva de pila: se prohibe cargar en las ultimas 50 posiciones
    // para garantizar espacio minimo para la pila del sistema.
    if (direccionFin > TAMANO_MEMORIA - 50) { 
        logLoader("ERROR: Intento de cargar en zona de PILA (1950-1999). Fin del programa: %d", direccionFin);
        printf("[LOADER] ERROR: No hay espacio seguro. Las ultimas 50 posiciones estan RESERVADAS para la Pila.\n");
        goto cleanup;
    }

    // verificar si hay datos preexistentes en el rango
    for (int k = 0; k < bufferLen; k++) {
        Palabra p = leerMemoria(direccionBase + k);
        if (p.digitos != 0 || p.signo != 0) {
            logLoader("ERROR: Memoria ocupada en direcccon %d. No se puede cargar.", direccionBase + k);
            printf("[LOADER] ERROR: Conflicto de memoria en direccion %d. Ya contiene datos.\n", direccionBase + k);
            goto cleanup; // salto al final para cerrar el archivo
        }
    }
    if (numeroPalabrasHeader != -1 && numeroPalabrasHeader != bufferLen) {
        logLoader("ERROR: .NumeroPalabras (%d) no coincide con instrucciones leidas (%d)",
               numeroPalabrasHeader, bufferLen);
        goto cleanup; // salto al final para cerrar el archivo
    }
    // validar _start (base 1, de 1 a bufferLen)
    if (lineaInicio < 1 || lineaInicio > bufferLen) {
        logLoader("ERROR: _start invalido o fuera de rango (debe ser 1..%d)", bufferLen);
        goto cleanup; // salto al final para cerrar el archivo
    }

    // escribir buffer a memoria (commit)
    int i;
    // direccionBase ya fue calculada arriba
    for (i = 0; i < bufferLen; i++) {
        escribirMemoria(direccionBase + i, buffer[i]);
        logLoader("Instruccion %d cargada en direccion %d: %d%07d",
               i, direccionBase + i, buffer[i].signo, buffer[i].digitos);
    }

    // todo lo que se obtuvo despues e leer y validar se guarda en la estructura
    strncpy(programaActual.nombre, nombrePrograma, MAX_NOMBRE_PROGRAMA - 1);
    programaActual.nombre[MAX_NOMBRE_PROGRAMA - 1] = '\0';
    programaActual.lineaInicio = lineaInicio - 1;  // Convertir a base 0
    programaActual.numeroPalabras = bufferLen;
    programaActual.direccionBase = direccionBase;
    programaActual.direccionLimite = direccionBase + bufferLen - 1;  // ultima instruccion

    logLoader("Programa '%s' cargado exitosamente", programaActual.nombre);
    logLoader("Instrucciones: %d, RB: %d, RL: %d, PC inicial: %d",
           bufferLen, programaActual.direccionBase, programaActual.direccionLimite, programaActual.lineaInicio);

    // Solo actualizar siguienteDireccionDisponible si estamos en modo automatico
    if (!modoManual) {
        siguienteDireccionDisponible = direccionBase + bufferLen;
    } else {
        // en modo manual, si cargamos "mas alla", podriamos actualizarla tambien para evitar huecos,
        // pero mejor dejarlo intacto o moverlo al final de lo nuevo si es mayor.
        // Por simplicidad, si es manual, no movemos el puntero automatico a menos que lo supere.
        if (direccionBase + bufferLen > siguienteDireccionDisponible) {
            siguienteDireccionDisponible = direccionBase + bufferLen;
        }
    }
    resultado = 0;  // exito

// usamos la salida correcta 
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

    // limpieza de estado de interrupciones
    // es fundamental limpiar cualquier interrupcion pendiente de ejecuciones anteriores
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
