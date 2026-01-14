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
    int lineaInicio = 0;
    int numeroPalabras = 0;
    char nombrePrograma[MAX_NOMBRE_PROGRAMA] = "";
    int instruccionesLeidas = 0;
    int direccionCarga;
    Palabra instruccion;
    long valorInstruccion;
    
    printf("[LOADER] Intentando cargar: %s\n", rutaArchivo);
    
    // Abrir el archivo
    archivo = fopen(rutaArchivo, "r");
    if (archivo == NULL) {
        printf("[LOADER] ERROR: No se pudo abrir el archivo %s\n", rutaArchivo);
        return 1;
    }
    
    // Leer el encabezado del programa
    while (fgets(linea, MAX_LINEA, archivo) != NULL) {
        // Eliminar salto de linea
        linea[strcspn(linea, "\n")] = '\0';
        
        // Ignorar lineas vacias y comentarios
        if (strlen(linea) == 0 || linea[0] == '/' || linea[0] == '#') {
            continue;
        }
        
        // Parsear _start
        if (strncmp(linea, "_start", 6) == 0) {
            sscanf(linea, "_start %d", &lineaInicio);
            printf("[LOADER] _start = %d\n", lineaInicio);
            continue;
        }
        
        // Parsear .NumeroPalabras
        if (strncmp(linea, ".NumeroPalabras", 15) == 0) {
            sscanf(linea, ".NumeroPalabras %d", &numeroPalabras);
            printf("[LOADER] NumeroPalabras = %d\n", numeroPalabras);
            continue;
        }
        
        // Parsear .NombreProg
        if (strncmp(linea, ".NombreProg", 11) == 0) {
            sscanf(linea, ".NombreProg %s", nombrePrograma);
            printf("[LOADER] NombreProg = %s\n", nombrePrograma);
            continue;
        }
        
        // Detectar fin del programa
        if (linea[0] == '.' && strlen(linea) == 1) {
            printf("[LOADER] Fin del programa detectado\n");
            break;
        }
        
        // Si llegamos aqui, es una instruccion (8 digitos)
        // Verificar que sea un numero
        if (linea[0] >= '0' && linea[0] <= '9') {
            // Convertir la linea a entero largo
            valorInstruccion = atol(linea);
            
            // Verificar espacio en memoria
            direccionCarga = siguienteDireccionDisponible + instruccionesLeidas;
            if (direccionCarga >= TAMANO_MEMORIA) {
                printf("[LOADER] ERROR: Memoria llena, no se puede cargar\n");
                fclose(archivo);
                return 1;
            }
            
            // Convertir a Palabra (signo-magnitud)
            // El primer digito es el signo, los 7 restantes la magnitud
            if (valorInstruccion >= 10000000) {
                // Tiene 8 digitos, el primero es signo
                instruccion.signo = valorInstruccion / 10000000;
                instruccion.digitos = valorInstruccion % 10000000;
            } else {
                // Menos de 8 digitos, signo positivo
                instruccion.signo = 0;
                instruccion.digitos = (int)valorInstruccion;
            }
            
            // Escribir en memoria
            escribirMemoria(direccionCarga, instruccion);
            
            printf("[LOADER] Instruccion %d cargada en direccion %d: %d%07d\n",
                   instruccionesLeidas, direccionCarga, 
                   instruccion.signo, instruccion.digitos);
            
            instruccionesLeidas++;
        }
    }
    
    fclose(archivo);
    
    // Verificar que se leyeron instrucciones
    if (instruccionesLeidas == 0) {
        printf("[LOADER] ERROR: No se encontraron instrucciones\n");
        return 1;
    }
    
    // Colocar centinela al final del programa
    direccionCarga = siguienteDireccionDisponible + instruccionesLeidas;
    instruccion.signo = CENTINELA_SIGNO;
    instruccion.digitos = CENTINELA_DIGITOS;
    escribirMemoria(direccionCarga, instruccion);
    printf("[LOADER] Centinela colocado en direccion %d\n", direccionCarga);
    
    // Guardar informacion del programa
    strncpy(programaActual.nombre, nombrePrograma, MAX_NOMBRE_PROGRAMA - 1);
    programaActual.lineaInicio = lineaInicio;
    programaActual.numeroPalabras = instruccionesLeidas;
    programaActual.direccionBase = siguienteDireccionDisponible;
    programaActual.direccionLimite = siguienteDireccionDisponible + instruccionesLeidas;
    
    printf("[LOADER] ============================================\n");
    printf("[LOADER] Programa '%s' cargado exitosamente\n", nombrePrograma);
    printf("[LOADER] Instrucciones: %d\n", instruccionesLeidas);
    printf("[LOADER] RB (direccion base): %d\n", programaActual.direccionBase);
    printf("[LOADER] RL (direccion limite): %d\n", programaActual.direccionLimite);
    printf("[LOADER] PC inicial (logico): %d\n", lineaInicio);
    printf("[LOADER] ============================================\n");
    
    // Actualizar siguiente direccion disponible para el proximo programa
    // Se coloca despues del centinela
    siguienteDireccionDisponible = direccionCarga + 1;
    
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
    registrosCpu.rl = programaActual.direccionLimite;
    
    // Establecer PC en la linea de inicio (direccion logica)
    registrosCpu.psw.pc = programaActual.lineaInicio;
    
    // Establecer pila al final del limite del programa
    // La pila crece hacia abajo desde RL
    registrosCpu.rx = programaActual.direccionLimite;
    registrosCpu.sp = programaActual.direccionLimite;
    
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
