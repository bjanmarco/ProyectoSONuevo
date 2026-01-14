/*
 * ============================================================================
 * ARCHIVO: main.c
 * DESCRIPCION: Punto de entrada principal de la maquina virtual.
 *              Implementa la consola interactiva para cargar y ejecutar
 *              programas en modo normal o debug.
 *              Segun especificaciones de prueba.txt seccion 6.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/hardware.h"
#include "../include/memoria.h"
#include "../include/disco.h"
#include "../include/dma.h"
#include "../include/loader.h"
#include "../include/logger.h"
#include "../include/cpu.h"

/* ============================================================================
 * CONSTANTES DE LA CONSOLA
 * ============================================================================ */

#define MAX_COMANDO 256     // Tamanio maximo de un comando
#define MAX_RUTA 256        // Tamanio maximo de una ruta de archivo

/* ============================================================================
 * VARIABLES GLOBALES
 * ============================================================================ */

// Modo de ejecucion: 0 = normal (run), 1 = debug
int modoDebug = 0;

// Indica si hay un programa cargado listo para ejecutar
int programaCargado = 0;

// Referencias externas a registros y variables del CPU
extern Registros registrosCpu;
extern int cpuEjecutando;
extern int contadorCiclos;

/* ============================================================================
 * PROTOTIPOS DE FUNCIONES LOCALES
 * ============================================================================ */

void mostrarBienvenida();
void mostrarAyuda();
void mostrarEstadoRegistros();
void mostrarEstadoSistema();
int ejecutarModoNormal();
int ejecutarModoDebug();
void limpiarPantalla();

/* ============================================================================
 * FUNCION PRINCIPAL
 * ============================================================================ */

int main(int argc, char *argv[]) {
    char comando[MAX_COMANDO];
    char rutaArchivo[MAX_RUTA];
    int salir = 0;
    
    // Ignorar argumentos por ahora
    (void)argc;
    (void)argv;
    
    // Limpiar pantalla y mostrar bienvenida
    limpiarPantalla();
    mostrarBienvenida();
    
    // Inicializar el logger
    if (inicializarLogger("maquina_virtual.log") != 0) {
        printf("ERROR: No se pudo inicializar el logger\n");
        return 1;
    }
    logSistema("Maquina virtual iniciada");
    
    // Inicializar todos los componentes de hardware
    printf("\n[SISTEMA] Inicializando componentes de hardware...\n");
    inicializarMemoria();
    logSistema("Memoria inicializada");
    
    inicializarDisco();
    logSistema("Disco inicializado");
    
    inicializarDma();
    logSistema("DMA inicializado");
    
    inicializarLoader();
    logSistema("Loader inicializado");
    
    inicializarCpu();
    logSistema("CPU inicializado");
    
    printf("[SISTEMA] Todos los componentes inicializados correctamente.\n\n");
    
    // Bucle principal de la consola
    while (!salir) {
        // Mostrar prompt
        printf("MV> ");
        fflush(stdout);
        
        // Leer comando
        if (fgets(comando, MAX_COMANDO, stdin) == NULL) {
            break;
        }
        
        // Eliminar salto de linea
        comando[strcspn(comando, "\n")] = '\0';
        
        // Ignorar comandos vacios
        if (strlen(comando) == 0) {
            continue;
        }
        
        logSistema("Comando recibido: %s", comando);
        
        // ================================================================
        // PROCESAR COMANDOS
        // ================================================================
        
        // Comando: salir / exit / quit
        if (strcmp(comando, "salir") == 0 || 
            strcmp(comando, "exit") == 0 || 
            strcmp(comando, "quit") == 0) {
            salir = 1;
            printf("Saliendo...\n");
            logSistema("Usuario solicito salir");
        }
        
        // Comando: ayuda / help
        else if (strcmp(comando, "ayuda") == 0 || 
                 strcmp(comando, "help") == 0) {
            mostrarAyuda();
        }
        
        // Comando: cargar <archivo>
        else if (strncmp(comando, "cargar ", 7) == 0) {
            strncpy(rutaArchivo, comando + 7, MAX_RUTA - 1);
            rutaArchivo[MAX_RUTA - 1] = '\0';
            
            printf("\n[LOADER] Cargando programa: %s\n", rutaArchivo);
            logLoader("Iniciando carga de: %s", rutaArchivo);
            
            if (cargarPrograma(rutaArchivo) == 0) {
                programaCargado = 1;
                printf("[LOADER] Programa cargado exitosamente.\n");
                printf("[LOADER] Use 'run' para ejecutar o 'debug' para depurar.\n\n");
                logLoader("Programa cargado exitosamente");
            } else {
                printf("[LOADER] ERROR: No se pudo cargar el programa.\n\n");
                logLoader("ERROR al cargar programa");
            }
        }
        
        // Comando: run (ejecutar en modo normal)
        else if (strcmp(comando, "run") == 0) {
            if (!programaCargado) {
                printf("ERROR: No hay programa cargado. Use 'cargar <archivo>' primero.\n\n");
            } else {
                printf("\n========================================\n");
                printf(" EJECUTANDO EN MODO NORMAL\n");
                printf("========================================\n\n");
                logSistema("Iniciando ejecucion en modo NORMAL");
                
                modoDebug = 0;
                prepararEjecucion();
                ejecutarModoNormal();
                
                printf("\n========================================\n");
                printf(" EJECUCION FINALIZADA\n");
                printf("========================================\n\n");
                logSistema("Ejecucion finalizada");
                
                // Programa terminado, permitir cargar otro
                programaCargado = 0;
            }
        }
        
        // Comando: debug (ejecutar en modo debug)
        else if (strcmp(comando, "debug") == 0) {
            if (!programaCargado) {
                printf("ERROR: No hay programa cargado. Use 'cargar <archivo>' primero.\n\n");
            } else {
                printf("\n========================================\n");
                printf(" EJECUTANDO EN MODO DEBUG\n");
                printf(" Comandos: [Enter]=siguiente, r=registros, c=continuar, q=salir\n");
                printf("========================================\n\n");
                logSistema("Iniciando ejecucion en modo DEBUG");
                
                modoDebug = 1;
                prepararEjecucion();
                ejecutarModoDebug();
                
                printf("\n========================================\n");
                printf(" EJECUCION FINALIZADA\n");
                printf("========================================\n\n");
                logSistema("Ejecucion en debug finalizada");
                
                programaCargado = 0;
            }
        }
        
        // Comando: registros / reg
        else if (strcmp(comando, "registros") == 0 || 
                 strcmp(comando, "reg") == 0) {
            mostrarEstadoRegistros();
        }
        
        // Comando: estado
        else if (strcmp(comando, "estado") == 0) {
            mostrarEstadoSistema();
        }
        
        // Comando: reiniciar
        else if (strcmp(comando, "reiniciar") == 0) {
            printf("[SISTEMA] Reiniciando maquina virtual...\n");
            logSistema("Reiniciando sistema");
            
            reiniciarLoader();
            inicializarCpu();
            programaCargado = 0;
            
            printf("[SISTEMA] Sistema reiniciado.\n\n");
            logSistema("Sistema reiniciado");
        }
        
        // Comando: limpiar / clear / cls
        else if (strcmp(comando, "limpiar") == 0 || 
                 strcmp(comando, "clear") == 0 ||
                 strcmp(comando, "cls") == 0) {
            limpiarPantalla();
        }
        
        // Comando no reconocido
        else {
            printf("Comando no reconocido: '%s'\n", comando);
            printf("Escriba 'ayuda' para ver los comandos disponibles.\n\n");
        }
    }
    
    // Finalizar componentes
    printf("\n[SISTEMA] Finalizando maquina virtual...\n");
    logSistema("Finalizando maquina virtual");
    
    finalizarMemoria();
    finalizarLogger();
    
    printf("[SISTEMA] Hasta luego!\n");
    
    return 0;
}

/* ============================================================================
 * FUNCIONES DE INTERFAZ
 * ============================================================================ */

/*
 * mostrarBienvenida
 * -----------------
 * Muestra el mensaje de bienvenida de la maquina virtual.
 */
void mostrarBienvenida() {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    MAQUINA VIRTUAL                           ║\n");
    printf("║              Sistema Operativo - UCV 2024                    ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Escriba 'ayuda' para ver los comandos disponibles.          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

/*
 * mostrarAyuda
 * ------------
 * Muestra la lista de comandos disponibles.
 */
void mostrarAyuda() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                   COMANDOS DISPONIBLES                       ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  cargar <archivo>  - Carga un programa desde archivo         ║\n");
    printf("║  run               - Ejecuta el programa en modo normal      ║\n");
    printf("║  debug             - Ejecuta el programa paso a paso         ║\n");
    printf("║  registros (reg)   - Muestra el estado de los registros      ║\n");
    printf("║  estado            - Muestra el estado del sistema           ║\n");
    printf("║  reiniciar         - Reinicia la maquina virtual             ║\n");
    printf("║  limpiar (clear)   - Limpia la pantalla                      ║\n");
    printf("║  ayuda (help)      - Muestra esta ayuda                      ║\n");
    printf("║  salir (exit)      - Sale de la maquina virtual              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

/*
 * mostrarEstadoRegistros
 * ----------------------
 * Muestra el estado actual de todos los registros del CPU.
 */
void mostrarEstadoRegistros() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                   REGISTROS DEL CPU                          ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  AC  = %c%07d                                               ║\n",
           registrosCpu.ac.signo ? '-' : '+', registrosCpu.ac.digitos);
    printf("║  PC  = %05d (logico)                                        ║\n",
           registrosCpu.psw.pc);
    printf("║  MAR = %c%07d                                               ║\n",
           registrosCpu.mar.signo ? '-' : '+', registrosCpu.mar.digitos);
    printf("║  MDR = %c%07d                                               ║\n",
           registrosCpu.mdr.signo ? '-' : '+', registrosCpu.mdr.digitos);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  IR: Opcode=%02d  Modo=%d  Valor=%05d                         ║\n",
           registrosCpu.ir.codigoOperacion,
           registrosCpu.ir.direccionamiento,
           registrosCpu.ir.valor);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  RB = %05d    RL = %05d                                     ║\n",
           registrosCpu.rb, registrosCpu.rl);
    printf("║  RX = %05d    SP = %05d                                     ║\n",
           registrosCpu.rx, registrosCpu.sp);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  PSW: CC=%d  Modo=%s  Int=%s                          ║\n",
           registrosCpu.psw.codigoCondicion,
           registrosCpu.psw.modoOperacion == MODO_KERNEL ? "KERNEL " : "USUARIO",
           registrosCpu.psw.habilitarInterrupciones ? "HAB  " : "DESHAB");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

/*
 * mostrarEstadoSistema
 * --------------------
 * Muestra el estado general del sistema.
 */
void mostrarEstadoSistema() {
    extern int siguienteDireccionDisponible;
    extern InfoPrograma programaActual;
    extern ControladorDma dma;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                   ESTADO DEL SISTEMA                         ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Memoria:                                                    ║\n");
    printf("║    - Siguiente direccion disponible: %05d                   ║\n",
           siguienteDireccionDisponible);
    printf("║    - Memoria usada: %d palabras                              ║\n",
           siguienteDireccionDisponible - INICIO_MEMORIA_USUARIO);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Programa actual:                                            ║\n");
    if (programaCargado) {
        printf("║    - Nombre: %-20s                          ║\n", programaActual.nombre);
        printf("║    - RB=%05d  RL=%05d  Instrucciones=%d                   ║\n",
               programaActual.direccionBase, 
               programaActual.direccionLimite,
               programaActual.numeroPalabras);
    } else {
        printf("║    - (ninguno cargado)                                       ║\n");
    }
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  DMA:                                                        ║\n");
    printf("║    - Estado: %s                                            ║\n",
           dma.ocupado ? "OCUPADO" : "LIBRE  ");
    printf("║    - Ultimo resultado: %s                                   ║\n",
           dma.estado == 0 ? "EXITO" : "ERROR");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

/*
 * limpiarPantalla
 * ---------------
 * Limpia la pantalla de la terminal.
 */
void limpiarPantalla() {
    printf("\033[2J\033[H");  // Secuencia ANSI para limpiar pantalla
}

/* ============================================================================
 * FUNCIONES DE EJECUCION
 * ============================================================================ */

/*
 * ejecutarModoNormal
 * ------------------
 * Ejecuta el programa cargado en modo normal (sin pausas).
 */
int ejecutarModoNormal() {
    logCpu("Iniciando ejecucion en modo normal");
    
    // Llamar a la funcion ejecutarCpu() del modulo cpu.c
    ejecutarCpu();
    
    logCpu("Ejecucion normal finalizada");
    return 0;
}

/*
 * ejecutarModoDebug
 * -----------------
 * Ejecuta el programa cargado en modo debug (paso a paso).
 */
int ejecutarModoDebug() {
    char entrada[MAX_COMANDO];
    int continuar = 1;
    int ejecutarTodo = 0;  // Para comando 'c' (continuar sin pausar)
    int direccionFisica;
    
    logCpu("Iniciando ejecucion en modo debug");
    
    cpuEjecutando = 1;
    
    while (cpuEjecutando && continuar) {
        // Mostrar informacion de la instruccion actual
        direccionFisica = registrosCpu.psw.pc + INICIO_MEMORIA_USUARIO;
        
        printf("──────────────────────────────────────────────\n");
        printf(" PC (logico): %05d  |  PC (fisico): %05d\n", 
               registrosCpu.psw.pc, direccionFisica);
        printf(" AC: %c%07d  |  CC: %d\n",
               registrosCpu.ac.signo ? '-' : '+', 
               registrosCpu.ac.digitos,
               registrosCpu.psw.codigoCondicion);
        
        // Ejecutar un ciclo
        if (!cicloCpu()) {
            printf("\n[DEBUG] Programa finalizado.\n");
            break;
        }
        
        // Mostrar instruccion ejecutada
        printf(" Ejecutado: Op=%02d Dir=%d Val=%05d\n",
               registrosCpu.ir.codigoOperacion,
               registrosCpu.ir.direccionamiento,
               registrosCpu.ir.valor);
        
        logDebug("PC=%d Op=%02d Dir=%d Val=%05d AC=%d",
                 registrosCpu.psw.pc - 1,
                 registrosCpu.ir.codigoOperacion,
                 registrosCpu.ir.direccionamiento,
                 registrosCpu.ir.valor,
                 palabraAEntero(registrosCpu.ac));
        
        // Si estamos en modo "continuar", no pausar
        if (ejecutarTodo) {
            continue;
        }
        
        // Esperar comando del usuario
        printf("[DEBUG] > ");
        fflush(stdout);
        
        if (fgets(entrada, MAX_COMANDO, stdin) == NULL) {
            break;
        }
        
        entrada[strcspn(entrada, "\n")] = '\0';
        
        // Procesar comando de debug
        if (strlen(entrada) == 0) {
            // Enter: siguiente instruccion
            continue;
        } else if (entrada[0] == 'r' || strcmp(entrada, "reg") == 0) {
            // Mostrar registros
            mostrarEstadoRegistros();
        } else if (entrada[0] == 'c') {
            // Continuar sin pausar
            printf("[DEBUG] Continuando ejecucion...\n");
            ejecutarTodo = 1;
        } else if (entrada[0] == 'q' || strcmp(entrada, "salir") == 0) {
            // Salir del debug
            printf("[DEBUG] Deteniendo ejecucion.\n");
            continuar = 0;
            cpuEjecutando = 0;
        } else if (entrada[0] == 'm') {
            // Mostrar memoria (direccion especificada)
            int dir = 0;
            if (sscanf(entrada, "m %d", &dir) == 1) {
                Palabra p = leerMemoria(dir);
                printf("[DEBUG] Memoria[%d] = %c%07d\n", 
                       dir, p.signo ? '-' : '+', p.digitos);
            } else {
                printf("[DEBUG] Uso: m <direccion>\n");
            }
        } else if (entrada[0] == 'h' || entrada[0] == '?') {
            // Ayuda del debug
            printf("\n[DEBUG] Comandos disponibles:\n");
            printf("  [Enter] - Ejecutar siguiente instruccion\n");
            printf("  r       - Mostrar registros\n");
            printf("  c       - Continuar (sin pausar)\n");
            printf("  m <dir> - Mostrar contenido de memoria\n");
            printf("  q       - Salir del debug\n");
            printf("  h / ?   - Mostrar esta ayuda\n\n");
        }
    }
    
    logCpu("Ejecucion debug finalizada");
    return 0;
}
