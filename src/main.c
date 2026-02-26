// punto de entrada principal de la maquina virtual
// implementa la consola interactiva para cargar y ejecutar programas en modo normal o debug
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
#include "../include/proceso.h"

// constantes de la consola
#define MAX_COMANDO 256     // tamanio maximo de un comando
#define MAX_RUTA 256        // tamanio maximo de una ruta de archivo

// variables globales
// modo de ejecucion: 0 = normal (run), 1 = debug
int modoDebug = 0;

// indica si hay un programa cargado listo para ejecutar
int programaCargado = 0;
int pidProcesoCargado = -1;

// referencias externas a registros y variables del CPU
extern Registros registrosCpu;
extern int cpuEjecutando;
extern int contadorCiclos;

// prototipos de funciones locales
void mostrarAyuda();
void mostrarEstadoRegistros();
int ejecutarModoNormal();
int ejecutarModoDebug();

// funcion principal
int main(int argc, char *argv[]) {
    char comando[MAX_COMANDO];
    char rutaArchivo[MAX_RUTA];
    int salir = 0;
    
    // ignorar argumentos por ahora
    (void)argc;
    (void)argv;
    
    // inicializar el logger
    if (inicializarLogger("maquina_virtual.log") != 0) {
        printf("ERROR: No se pudo inicializar el logger\n");
        return 1;
    }
    logSistema("Maquina virtual iniciada");
    
    // inicializar todos los componentes de hardware
    // inicializar todos los componentes de hardware (Simulacion de Bootstrap)
    printf("\n INICIANDO BOOTSTRAP DEL SO \n\n");
    printf("[BOOTSTRAP] Cargando nucleo y componentes\n");
    
    inicializarMemoria();
    logSistema("Memoria inicializada (KERNEL)");
    
    inicializarDisco();
    logSistema("Disco inicializado (KERNEL)");
    
    inicializarDma();
    logSistema("DMA inicializado (KERNEL)");
    
    inicializarLoader();
    logSistema("Loader inicializado (KERNEL)");

    inicializarGestorProcesos();
    logSistema("Gestor de procesos inicializado (KERNEL)");
    
    inicializarCpu();
    logSistema("CPU inicializado (KERNEL)");
    
    printf("[BOOTSTRAP] Componentes de hardware verificados OK.\n");
    
    // cambiar a modo USUARIO
    registrosCpu.psw.modoOperacion = MODO_USUARIO;
    logSistema("Sistema cambiado a modo USUARIO");
    
    printf("[BOOTSTRAP] Sistema Operativo listo. Sesion de usuario iniciada.\n");
    
    // mostrar comandos disponibles antes de dar control
    mostrarAyuda();
    
    // bucle principal de la consola
    while (!salir) {
        // mostrar prompt
        printf("MV> ");
        fflush(stdout);
        
        // leer comando
        if (fgets(comando, MAX_COMANDO, stdin) == NULL) {
            break;
        }
        
        // eliminar salto de linea
        comando[strcspn(comando, "\n")] = '\0';
        
        // ignorar comandos vacios
        if (strlen(comando) == 0) {
            continue;
        }
        
        logSistema("Comando recibido: %s", comando);
        
        // procesar comandos
        
        // comando: salir / exit / quit
        if (strcmp(comando, "salir") == 0 || 
            strcmp(comando, "exit") == 0 || 
            strcmp(comando, "quit") == 0) {
            salir = 1;
            printf("Saliendo...\n");
            logSistema("Usuario solicito salir");
        }
        
        // comando: ayuda / help
        else if (strcmp(comando, "ayuda") == 0 || 
                 strcmp(comando, "help") == 0) {
            mostrarAyuda();
        }
        
        // comando: cargar <archivo> [direccion]
        else if (strncmp(comando, "cargar ", 7) == 0) {
            char rutTemp[MAX_RUTA];
            int dirTemp = -1;
            int params = sscanf(comando + 7, "%s %d", rutTemp, &dirTemp);

            if (cantidadProcesosActivos() >= MAX_PROCESOS) {
                printf("[PROC] ERROR: Limite de procesos alcanzado (%d).\n\n", MAX_PROCESOS);
                logSistema("[PROC] Rechazado comando cargar: capacidad de procesos agotada");
                continue;
            }
            
            if (params >= 1) {
                strncpy(rutaArchivo, rutTemp, MAX_RUTA - 1);
                rutaArchivo[MAX_RUTA - 1] = '\0';
                
                logLoader("Iniciando carga de: %s (Dir: %d)", rutaArchivo, dirTemp);
                
                if (cargarPrograma(rutaArchivo, dirTemp) == 0) {
                    pidProcesoCargado = crearProceso(
                        programaActual.nombre,
                        programaActual.direccionBase,
                        programaActual.direccionLimite,
                        programaActual.lineaInicio
                    );
                    if (pidProcesoCargado < 0) {
                        printf("[LOADER] ERROR: No se pudo crear el BCP del proceso.\n\n");
                        logSistema("ERROR creando proceso para programa %s", programaActual.nombre);
                        programaCargado = 0;
                    } else {
                        cambiarEstadoProceso(pidProcesoCargado, ESTADO_LISTO, "Programa cargado y admitido");
                        programaCargado = 1;
                    }

                    if (!programaCargado) {
                        continue;
                    }

                    programaCargado = 1;
                    printf("[LOADER] Programa cargado exitosamente.\n");
                    printf("[PROC] Proceso PID=%d en estado LISTO.\n", pidProcesoCargado);
                    printf("[LOADER] Use 'run' para ejecutar o 'debug' para depurar.\n\n");
                    logLoader("Programa cargado exitosamente");
                } else {
                    printf("[LOADER] ERROR: No se pudo cargar el programa.\n\n");
                    logLoader("ERROR al cargar programa");
                }
            } else {
                printf("Uso: cargar <archivo> [direccion_memoria]\n");
            }
        }
        
        // comando: run (ejecutar en modo normal)
        else if (strcmp(comando, "run") == 0) {
            if (!programaCargado) {
                printf("ERROR: No hay programa cargado. Use 'cargar <archivo>' primero.\n\n");
            } else {

                printf("[SISTEMA] Ejecutando programa...\n");
                logSistema("Iniciando ejecucion en modo NORMAL");

                if (pidProcesoCargado >= 0) {
                    cambiarEstadoProceso(pidProcesoCargado, ESTADO_EJECUCION, "Despachado a CPU (run)");
                }
                
                modoDebug = 0;
                prepararEjecucion();
                if (pidProcesoCargado >= 0) {
                    actualizarContextoProceso(pidProcesoCargado, &registrosCpu);
                }
                ejecutarModoNormal();
                if (pidProcesoCargado >= 0) {
                    actualizarContextoProceso(pidProcesoCargado, &registrosCpu);
                    cambiarEstadoProceso(pidProcesoCargado, ESTADO_TERMINADO, "Fin de ejecucion run");
                }
                
                printf("[SISTEMA] Ejecucion finalizada.\n\n");
                logSistema("Ejecucion finalizada");
                
                // programa terminado, permitir cargar otro
                programaCargado = 0;
                pidProcesoCargado = -1;
            }
        }
        
        // comando: debug (ejecutar en modo debug)
        else if (strcmp(comando, "debug") == 0) {
            if (!programaCargado) {
                printf("ERROR: No hay programa cargado. Use 'cargar <archivo>' primero.\n\n");
            } else {
                printf(" EJECUTANDO EN MODO DEBUG\n");
                printf(" Comandos: [Enter]=siguiente, r=registros, h=ayuda, q=salir\n");
                logSistema("Iniciando ejecucion en modo DEBUG");

                if (pidProcesoCargado >= 0) {
                    cambiarEstadoProceso(pidProcesoCargado, ESTADO_EJECUCION, "Despachado a CPU (debug)");
                }
                
                modoDebug = 1;
                prepararEjecucion();
                if (pidProcesoCargado >= 0) {
                    actualizarContextoProceso(pidProcesoCargado, &registrosCpu);
                }
                ejecutarModoDebug();
                if (pidProcesoCargado >= 0) {
                    actualizarContextoProceso(pidProcesoCargado, &registrosCpu);
                    cambiarEstadoProceso(pidProcesoCargado, ESTADO_TERMINADO, "Fin de ejecucion debug");
                }
                
                printf(" EJECUCION FINALIZADA\n\n");
                logSistema("Ejecucion en debug finalizada");
                
                programaCargado = 0;
                pidProcesoCargado = -1;
            }
        }
        
        // comando: registros / reg
        else if (strcmp(comando, "registros") == 0 || 
                 strcmp(comando, "reg") == 0) {
            mostrarEstadoRegistros();
        }
        

        
        // comando no reconocido
        else {
            printf("Comando no reconocido: '%s'\n", comando);
            printf("Escriba 'ayuda' para ver los comandos disponibles.\n\n");
        }
    }
    
    // finalizar componentes
    printf("\n[SISTEMA] Finalizando maquina virtual...\n");
    logSistema("Finalizando maquina virtual");
    
    finalizarDma();
    finalizarMemoria();
    finalizarLogger();
    
    printf("[SISTEMA] Hasta luego!\n");
    
    return 0;
}

// funciones de interfaz
// muestra la lista de comandos disponibles.
void mostrarAyuda() {
    printf("\n");
    printf(" COMANDOS DISPONIBLES \n");
    printf(" cargar <archivo> [dir] - Carga un programa (dir opcional)        \n");
    printf(" run                    - Ejecuta el programa en modo normal      \n");
    printf(" debug                  - Ejecuta el programa en modo debug       \n");
    printf(" registros (reg)        - Muestra los registros                   \n");
    printf(" ayuda (help)           - Comandos                                \n");
    printf(" salir (exit)           - Salir                                   \n");
    printf("\n");
}

// muestra el estado actual de todos los registros del CPU.
void mostrarEstadoRegistros() {
    printf("\n");
    printf(" REGISTROS DEL CPU \n");
    printf(" AC  = %c%07d \n",
           registrosCpu.ac.signo ? '-' : '+', registrosCpu.ac.digitos);
    printf(" PC  = %05d (logico) \n",
           registrosCpu.psw.pc);
    printf(" MAR = %c%07d \n",
           registrosCpu.mar.signo ? '-' : '+', registrosCpu.mar.digitos);
    printf(" MDR = %c%07d \n",
           registrosCpu.mdr.signo ? '-' : '+', registrosCpu.mdr.digitos);
    printf(" IR: Opcode=%02d  Modo=%d  Valor=%05d \n",
           registrosCpu.ir.codigoOperacion,
           registrosCpu.ir.direccionamiento,
           registrosCpu.ir.valor);
    printf(" RB = %05d    RL = %05d \n",
           registrosCpu.rb, registrosCpu.rl);
    printf(" RX = %05d    SP = %05d \n",
           registrosCpu.rx, registrosCpu.sp);
    printf(" PSW: CC=%d  Modo=%s  Int=%s \n",
           registrosCpu.psw.codigoCondicion,
           registrosCpu.psw.modoOperacion == MODO_KERNEL ? "KERNEL " : "USUARIO",
           registrosCpu.psw.habilitarInterrupciones ? "HAB  " : "DESHAB");
    printf("\n");
}

// funciones de ejecucion

// ejecuta el programa cargado en modo normal (sin pausas)
int ejecutarModoNormal() {
    logCpu("Iniciando ejecucion en modo normal");
    
    // llamar a la funcion ejecutarCpu() del modulo cpu.c
    ejecutarCpu();
    
    logCpu("Ejecucion normal finalizada");
    return 0;
}

// ejecuta el programa cargado en modo debug (paso a paso).
int ejecutarModoDebug() {
    char entrada[MAX_COMANDO];
    int continuar = 1;
    int direccionFisica;

    logCpu("Iniciando ejecucion en modo debug");
    
    cpuEjecutando = 1;
    
    // bucle principal del debugger
    while (cpuEjecutando && continuar) {
        direccionFisica = registrosCpu.psw.pc + registrosCpu.rb;

        // mostrar informacion de la instruccion proxima a ejecutar
        printf("──────────────────────────────────────────────\n");
        printf(" Proxima instruccion:\n");
        printf(" PC (logico): %05d  |  Dir Fisica: %05d\n",
               registrosCpu.psw.pc, direccionFisica);
        printf(" AC: %c%07d  |  CC: %d\n",
               registrosCpu.ac.signo ? '-' : '+',
               registrosCpu.ac.digitos,
               registrosCpu.psw.codigoCondicion);
        printf("──────────────────────────────────────────────\n");
        
        // bucle para procesar comandos hasta que el usuario quiera avanzar
            while (1) {
                printf("[DEBUG] > ");
                fflush(stdout);
                
                if (fgets(entrada, MAX_COMANDO, stdin) == NULL) {
                    continuar = 0;
                    break;
                }
                
                entrada[strcspn(entrada, "\n")] = '\0';
                
                // procesar comando de debug
                if (strlen(entrada) == 0 || entrada[0] == 's') {
                    break;  // Salir del bucle de comandos para ejecutar
                    
                } else if (entrada[0] == 'r' || strcmp(entrada, "reg") == 0) {
                    // mostrar registros
                    mostrarEstadoRegistros();
                    
                } else if (entrada[0] == 'q' || strcmp(entrada, "salir") == 0) {
                    // salir del debug
                    printf("[DEBUG] Deteniendo ejecucion.\n");
                    continuar = 0;
                    cpuEjecutando = 0;
                    break;
                    
                } else if (entrada[0] == 'h' || entrada[0] == '?') {
                    // mostrar ayuda del debug
                    printf("\n[DEBUG] Comandos disponibles:\n");
                    printf("  [Enter] / s - Ejecutar siguiente instruccion\n");
                    printf("  r           - Mostrar registros del CPU\n");
                    printf("  h / ?       - Mostrar esta ayuda\n");
                    printf("  q           - Salir del modo debug\n\n");
                    
                } else {
                    // comando no reconocido
                    printf("[DEBUG] Comando no reconocido. Use 'h' para ayuda.\n");
                }
            }
        
        // si el usuario quiere salir, no ejecutar mas
        if (!continuar || !cpuEjecutando) {
            break;
        }
        
        // ejecutar un ciclo de CPU
        if (!cicloCpu()) {
            printf("\n[DEBUG] Programa finalizado.\n");
            break;
        }
        
        // mostrar instruccion que se acaba de ejecutar
        printf(" >> Ejecutado: Op=%02d Dir=%d Val=%05d\n",
               registrosCpu.ir.codigoOperacion,
               registrosCpu.ir.direccionamiento,
               registrosCpu.ir.valor);
        
        logDebug("PC=%d Op=%02d Dir=%d Val=%05d AC=%d",
                 registrosCpu.psw.pc - 1,
                 registrosCpu.ir.codigoOperacion,
                 registrosCpu.ir.direccionamiento,
                 registrosCpu.ir.valor,
                 palabraAEntero(registrosCpu.ac));
    }
    
    logCpu("Ejecucion debug finalizada");
    return 0;
}
