// Punto de entrada principal y consola de la maquina virtual
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
#include "../include/procesos.h"


#define MAX_COMANDO 256
#define MAX_RUTA 256

int modoDebug = 0;

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
    int salir = 0;
    
    (void)argc;
    (void)argv;
    
    // inicializar el logger
    if (inicializarLogger("maquina_virtual.log") != 0) {
        printf("ERROR: No se pudo inicializar el logger\n");
        return 1;
    }
    logSistema("Maquina virtual iniciada");
    
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
    
    inicializarCpu();
    logSistema("CPU inicializado (KERNEL)");

    inicializarGestorProcesos();
    logSistema("Gestor de Procesos (BCP) inicializado (KERNEL)");
    
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
        
        // Comando salir
        if (strcmp(comando, "salir") == 0 || 
            strcmp(comando, "exit") == 0 || 
            strcmp(comando, "quit") == 0 ||
            strcmp(comando, "apagar") == 0) {
            salir = 1;
            printf("Saliendo...\n");
            logSistema("Usuario solicito apagar/salir");
        }
        
        // comando: ayuda / help
        else if (strcmp(comando, "ayuda") == 0 || 
                 strcmp(comando, "help") == 0) {
            mostrarAyuda();
        }
        
        // comando: reiniciar
        else if (strcmp(comando, "reiniciar") == 0) {
            printf("\n[SISTEMA] Reiniciando maquina virtual...\n");
            logSistema("Usuario solicito reiniciar");
            
            inicializarMemoria();
            inicializarDma();
            inicializarLoader();
            inicializarCpu();
            inicializarGestorProcesos();
            
            registrosCpu.psw.modoOperacion = MODO_USUARIO;
            
            printf("[BOOTSTRAP] Sistema Operativo reiniciado exitosamente.\n\n");
        }
        
        // Comando ejecutar
        else if (strncmp(comando, "ejecutar ", 9) == 0) {
            // Limpiar historial de procesos anteriores en PS
            limpiarProcesosTerminados();
            
            char *copiaComando = strdup(comando + 9);
            char *programa = strtok(copiaComando, " ");
            
            int programasCargadosExtosamente = 0;

            // Recorremos todos los parametros enviados por espacio
            while (programa != NULL) {
                // Limpiar posibles saltos de linea accidentales si es el ultimo parametro
                programa[strcspn(programa, "\r\n")] = '\0';
                
                if (strlen(programa) > 0) {
                    // Agregar extension .txt si no la tiene
                    char rutaConExtension[MAX_RUTA];
                    if (strstr(programa, ".txt") == NULL) {
                        snprintf(rutaConExtension, sizeof(rutaConExtension), "%s.txt", programa);
                    } else {
                        strncpy(rutaConExtension, programa, MAX_RUTA);
                    }
                    
                    logLoader("Usuario solicito cargar programa: %s", rutaConExtension);
                    printf("[SISTEMA] Cargando %s...\n", rutaConExtension);

                    // Arquitectura de Memoria (Disco -> RAM)
                    
                    // Paso 1: Intentar cargarlo en Disco
                    if (cargarProgramaEnDisco(rutaConExtension) == 0) {
                        
                        // Paso 2: Volcarlo desde el Disco hacia la Memoria RAM
                        if (cargarProgramaEnMemoria(rutaConExtension) == 0) {
                            programasCargadosExtosamente++;
                        } else {
                            printf("[ERROR SO] Fallo al extraer '%s' desde el Disco a la RAM.\n", rutaConExtension);
                        }
                        
                    } else {
                        printf("[ERROR SO] Fallo crítico al escribir '%s' en el Disco Duro. Verifique sintaxis.\n", rutaConExtension);
                    }
                }
                programa = strtok(NULL, " ");
            }
            free(copiaComando);

            // Iniciar RoundRobin
            if (programasCargadosExtosamente > 0) {
                printf("[SISTEMA] %d programa(s) cargado(s) exitosamente.\n", programasCargadosExtosamente);
                printf("[SISTEMA] Iniciando ejecucion (Turno Rotatorio)...\n\n");
                logSistema("Iniciando ejecucion de CPU Planificada");
                
                // Limpiar contexto muerto
                extern int procesoEnEjecucion;
                procesoEnEjecucion = -1;
                int primerProceso = planificarSiguienteProceso();
                if (primerProceso != -1) {
                    despacharProceso(primerProceso);
                }
                
                ejecutarModoNormal();
                
                printf("\n[SISTEMA] Ejecucion finalizada.\n\n");
                logSistema("Ejecucion planificada finalizada");
            } else {
                printf("[SISTEMA] No se pudo cargar ningun programa para ejecutar.\n");
            }
        }
        
        // Comando registros
        else if (strcmp(comando, "registros") == 0 || 
                 strcmp(comando, "reg") == 0) {
            mostrarEstadoRegistros();
        }
        
        // Comando memstat
        else if (strcmp(comando, "memstat") == 0) {
            mostrarEstadisticasMemoria();
        }
        
        // Comando ps
        else if (strcmp(comando, "ps") == 0) {
            mostrarTablaProcesos();
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
    
    finalizarMemoria();
    finalizarLogger();
    
    printf("[SISTEMA] Hasta luego!\n");
    
    return 0;
}

// Funciones de interfaz
void mostrarAyuda() {
    printf("\n");
    printf(" COMANDOS DISPONIBLES \n");
    printf(" ejecutar <p1> <p2>...  - Carga los programas en BCP y los ejecuta\n");
    printf(" memstat                - Estadisticas de uso de la Memoria Principal\n");
    printf(" ps                     - Muestra la tabla de procesos del Sistema\n");
    printf(" registros (reg)        - Muestra los registros                   \n");
    printf(" reiniciar              - Reinicia la maquina virtual             \n");
    printf(" ayuda (help)           - Muestra los comandos disponibles        \n");
    printf(" salir (exit, apagar)   - Salir del simulador                     \n");
    printf("\n");
}

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

// Funciones de ejecucion

int ejecutarModoNormal() {
    logCpu("Iniciando ejecucion en modo normal");
    
    // llamar a la funcion ejecutarCpu() del modulo cpu.c
    ejecutarCpu();
    
    logCpu("Ejecucion normal finalizada");
    return 0;
}

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
        
        // Comandos del depurador
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
