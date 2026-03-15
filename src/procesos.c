#include <stdio.h>
#include <string.h>

#include "../include/procesos.h"
#include "../include/logger.h"
#include "../include/cpu.h" // Para constantes de offset
#include "../include/memoria.h" // Para leerMemoria en comando ps

// Variables globales
BCP tablaProcesos[MAX_PROCESOS];
int numProcesosActivos = 0;
int procesoEnEjecucion = -1; 
int contadorIdProcesos = 1;

// Inicializar tabla de procesos
void inicializarGestorProcesos() {
    numProcesosActivos = 0;
    procesoEnEjecucion = -1;
    contadorIdProcesos = 1;
    
    // Limpiar BCP
    for (int i = 0; i < MAX_PROCESOS; i++) {
        tablaProcesos[i].id = -1;
        tablaProcesos[i].estado = ESTADO_TERMINADO;
        tablaProcesos[i].ticsDormido = 0;
    }
}

// Limpiar procesos terminados de la vista
void limpiarProcesosTerminados() {
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].estado == ESTADO_TERMINADO) {
            tablaProcesos[i].id = -1;
        }
    }
}

// Retornar indice disponible
int buscarEspacioLibreBCP() {
    if (numProcesosActivos >= MAX_PROCESOS) return -1;
    
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].id == -1 || tablaProcesos[i].estado == ESTADO_TERMINADO) {
            return i;
        }
    }
    return -1;
}

// Crear nuevo proceso
int crearProceso(const char *nombrePrograma, int dirBase, int dirLimite, int pcInicial, int tamanoCodigo, int requiereKernel) {
    if (numProcesosActivos >= MAX_PROCESOS) {
        printf("[ERROR SO] No hay mas espacio en la tabla de procesos (Maximo estricto 20).\n");
        return -1;
    }
    
    int indiceLibre = buscarEspacioLibreBCP();
    if(indiceLibre == -1) return -1;
    
    BCP *nuevoP = &tablaProcesos[indiceLibre];
    
    nuevoP->id = contadorIdProcesos++;
    strncpy(nuevoP->nombre, nombrePrograma, MAX_NOMBRE_PROGRAMA - 1);
    nuevoP->nombre[MAX_NOMBRE_PROGRAMA - 1] = '\0';
    
    nuevoP->direccionBase = dirBase;
    nuevoP->direccionLimite = dirLimite;
    nuevoP->tamanoCodigo = tamanoCodigo;
    nuevoP->requiereKernel = requiereKernel;
    
    // Configurar contexto inicial basico
    if (requiereKernel) {
        nuevoP->contexto.psw.modoOperacion = MODO_KERNEL;
        logSistema("Proceso [%s] (ID: %d) configurado para inyectarse en MODO KERNEL (Contiene intruccion privilegiada)", nuevoP->nombre, nuevoP->id);
    } else {
        nuevoP->contexto.psw.modoOperacion = MODO_USUARIO;
        logSistema("Proceso [%s] (ID: %d) configurado para MODO USUARIO", nuevoP->nombre, nuevoP->id);
    }
    
    nuevoP->contexto.psw.habilitarInterrupciones = INT_HABILITADAS;
    nuevoP->contexto.psw.codigoCondicion = CC_CERO;
    nuevoP->contexto.psw.pc = pcInicial; 
    nuevoP->contexto.rb = dirBase;
    nuevoP->contexto.rl = dirLimite;

    nuevoP->contexto.rx = dirLimite;
    nuevoP->contexto.sp = dirLimite;

    // Iniciar el estado logico
    nuevoP->estado = ESTADO_NUEVO;
    
    // Registrar en log
    logSistema("Proceso [%s] (ID: %d) creado -> Estado NUEVO", nuevoP->nombre, nuevoP->id);
    
    numProcesosActivos++;
    
    // Pasar a LISTO
    cambiarEstadoProceso(nuevoP->id, ESTADO_LISTO);
    
    return nuevoP->id;
}

// Retornar nombre del estado
const char* nombreDelEstado(int estado) {
    switch (estado) {
        case ESTADO_NUEVO: return "NUEVO";
        case ESTADO_LISTO: return "LISTO";
        case ESTADO_EJECUCION: return "EN EJECUCION";
        case ESTADO_DORMIDO: return "DORMIDO";
        case ESTADO_TERMINADO: return "TERMINADO";
        default: return "DESCONOCIDO";
    }
}

// Cambiar estado
void cambiarEstadoProceso(int idProceso, int nuevoEstado) {
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].id == idProceso) {
            
            // Evitar modificaciones si ya termino
            if (tablaProcesos[i].estado == ESTADO_TERMINADO) return;
            
            int viejoEstado = tablaProcesos[i].estado;
            tablaProcesos[i].estado = nuevoEstado;
            
            // Log
            logSistema("Proceso (ID: %d) cambio estado %s -> %s", 
                       idProceso, nombreDelEstado(viejoEstado), nombreDelEstado(nuevoEstado));
                       
            if (nuevoEstado == ESTADO_TERMINADO) {
                numProcesosActivos--;
            }
            return;
        }
    }
}

// Destruir proceso
void destruirProceso(int idProceso) {
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].id == idProceso) {
            // Mantener datos para el reporte final
            break;
        }
    }
    cambiarEstadoProceso(idProceso, ESTADO_TERMINADO);
}

// Actualizar procesos dormidos
void actualizarProcesosDormidos() {
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].estado == ESTADO_DORMIDO) {
            tablaProcesos[i].ticsDormido--;
            if (tablaProcesos[i].ticsDormido <= 0) {
                tablaProcesos[i].ticsDormido = 0;
                logSistema("Proceso (ID: %d) desperto de su Syscall SLEEP. Pasa a LISTO.", tablaProcesos[i].id);
                cambiarEstadoProceso(tablaProcesos[i].id, ESTADO_LISTO);
            }
        }
    }
}

// Verificar si hay procesos vivos
int hayProcesosVivos() {
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (i < numProcesosActivos) {
            if (tablaProcesos[i].estado != ESTADO_TERMINADO) return 1;
        }
    }
    return 0;
}

// Planificador Round Robin
// Retorna ID del proceso o -1 si no hay listos
int planificarSiguienteProceso() {
    int idCandidato = -1;
    int inicioBusqueda = 0;

    // Buscar a partir del proceso en ejecucion
    if (procesoEnEjecucion != -1) {
        for (int i = 0; i < MAX_PROCESOS; i++) {
            if (tablaProcesos[i].id == procesoEnEjecucion) {
                inicioBusqueda = (i + 1) % MAX_PROCESOS;
                break;
            }
        }
    }

    // Buscar proximo proceso LISTO
    for (int j = 0; j < MAX_PROCESOS; j++) {
        int indiceEvaluar = (inicioBusqueda + j) % MAX_PROCESOS;
        
        if (tablaProcesos[indiceEvaluar].id != -1 && 
            tablaProcesos[indiceEvaluar].estado == ESTADO_LISTO) {
            idCandidato = tablaProcesos[indiceEvaluar].id;
            break;
        }
    }
    
    // Verificar si el actual es el unico habilitado para seguir ejecutandose
    if (idCandidato == -1 && procesoEnEjecucion != -1) {
        for (int i = 0; i < MAX_PROCESOS; i++) {
            if (tablaProcesos[i].id == procesoEnEjecucion && 
                tablaProcesos[i].estado == ESTADO_EJECUCION) {
                idCandidato = procesoEnEjecucion;
                break;
            }
        }
    }

    return idCandidato;
}

// Despachar proceso (Intercambio de contexto)
void despacharProceso(int idNuevoProceso) {
    int indiceViejo = -1;
    if (procesoEnEjecucion != -1) {
        for (int i = 0; i < MAX_PROCESOS; i++) {
            if (tablaProcesos[i].id == procesoEnEjecucion) {
                indiceViejo = i;
                if (tablaProcesos[i].estado == ESTADO_EJECUCION || 
                    tablaProcesos[i].estado == ESTADO_DORMIDO ||
                    tablaProcesos[i].estado == ESTADO_TERMINADO) {
                    
                    // Solo si lo expulsamos a la fuerza (por timer), lo movemos a LISTO.
                    // Si ya estaba DORMIDO o TERMINADO por una Syscall, respetamos su estado.
                    if (tablaProcesos[i].estado == ESTADO_EJECUCION) {
                        cambiarEstadoProceso(procesoEnEjecucion, ESTADO_LISTO);
                    }

                    // Extraccion de estado real
                    decodificarPsw(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
                    int spOriginal = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
                    registrosCpu.rx = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
                    registrosCpu.rl = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
                    registrosCpu.rb = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
                    registrosCpu.ac = leerMemoria(registrosCpu.sp); registrosCpu.sp++;
                    
                    // Restaurar SP
                    registrosCpu.sp = spOriginal;
                    
                    // Guardar contexto en BCP
                    tablaProcesos[i].contexto = registrosCpu;
                }
                break;
            }
        }
    }

    // Cpu ocioso
    if (idNuevoProceso == -1) {
        procesoEnEjecucion = -1;
        return;
    }

    // Montar nuevo proceso
    int indiceNuevo = -1;
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].id == idNuevoProceso) {
            indiceNuevo = i;
            break;
        }
    }

    if (indiceNuevo != -1) {
        cambiarEstadoProceso(idNuevoProceso, ESTADO_EJECUCION);
        
        registrosCpu = tablaProcesos[indiceNuevo].contexto;
        
        // Limpieza fundamental
        extern int interrupcionPendiente;
        extern int interrupcionesPendientes[NUM_INTERRUPCIONES];
        extern int contadorCiclos;
        extern void reiniciarDeteccionBucle();
        
        interrupcionPendiente = 0;
        for(int i=0; i<NUM_INTERRUPCIONES; i++) interrupcionesPendientes[i] = 0;
        contadorCiclos = 0;
        reiniciarDeteccionBucle();
        
        procesoEnEjecucion = idNuevoProceso;

        if (indiceViejo != -1 && tablaProcesos[indiceViejo].id != idNuevoProceso && tablaProcesos[indiceViejo].estado == ESTADO_LISTO) {
            logSistema(">>> FIN QUANTUM: Saliente Proceso ID %d | Entrante Proceso ID %d <<<", 
                       tablaProcesos[indiceViejo].id, idNuevoProceso);
        } else {
             logSistema(">>> DESPACHO: Entrante Proceso ID %d <<<", idNuevoProceso);
        }
    }
}

// Diagnostico 
// Fase 2 - Comandos

void mostrarTablaProcesos() {
    printf("\nPROCESOS EN EL SISTEMA\n");
    printf("====================================================================================================\n");
    printf("%-4s | %-10s | %-9s | %-16s | %-13s | %-5s | %-5s | %s\n", 
           "PID", "ESTADO", "% MEMORIA", "RANGO (BASE-LIM)", "INSTRUCCIONES", "DATOS", "TOTAL", "NOMBRE");
    printf("----------------------------------------------------------------------------------------------------\n");
    
    int procesosMostrados = 0;
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].id != -1) {
            char descEstado[20];
            switch (tablaProcesos[i].estado) {
                case ESTADO_NUEVO: strcpy(descEstado, "NUEVO"); break;
                case ESTADO_LISTO: strcpy(descEstado, "LISTO"); break;
                case ESTADO_EJECUCION: strcpy(descEstado, "EN EJECUCION"); break;
                case ESTADO_DORMIDO: strcpy(descEstado, "DORMIDO"); break;
                case ESTADO_TERMINADO: strcpy(descEstado, "TERMINADO"); break;
                default: strcpy(descEstado, "DESCONOCIDO"); break;
            }
            
            int instrucciones = tablaProcesos[i].tamanoCodigo;
            int datosOcupados = 0;
            
            int inicioDatos = tablaProcesos[i].direccionBase + instrucciones;
            int limite = tablaProcesos[i].direccionLimite;
            
            for (int j = inicioDatos; j <= limite; j++) {
                Palabra p = leerMemoria(j);
                if (p.signo != 0 || p.digitos != 0) {
                    datosOcupados++;
                }
            }
            
            int subtotal = instrucciones + datosOcupados;
            float porcMem = ((float)subtotal / TAMANO_MEMORIA) * 100.0f;
            
            printf("%-4d | %-10s | %0.2f%%     | [%04d - %04d]    | %-13d | %-5d | %-5d | %s\n", 
                   tablaProcesos[i].id, 
                   descEstado, 
                   porcMem, 
                   tablaProcesos[i].direccionBase, 
                   tablaProcesos[i].direccionLimite, 
                   instrucciones, 
                   datosOcupados, 
                   subtotal,
                   tablaProcesos[i].nombre);
            procesosMostrados++;
        }
    }
    
    printf("====================================================================================================\n");
    printf("Total de procesos: %d\n\n", procesosMostrados);
}
