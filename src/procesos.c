#include <stdio.h>
#include <string.h>

#include "../include/procesos.h"
#include "../include/logger.h"
#include "../include/cpu.h" // Para constantes de offset

// Variables globales para la gestion
BCP tablaProcesos[MAX_PROCESOS];
int numProcesosActivos = 0;
int procesoEnEjecucion = -1; 
int contadorIdProcesos = 1; // Para que los IDs no empiecen desde 0 si no queremos

// Inicializa toda la tabla vacia limpiando posibles reciduos de basura de memoria (RAM)
void inicializarGestorProcesos() {
    numProcesosActivos = 0;
    procesoEnEjecucion = -1;
    contadorIdProcesos = 1;
    
    // Limpiamos los BCP
    for (int i = 0; i < MAX_PROCESOS; i++) {
        tablaProcesos[i].id = -1;
        tablaProcesos[i].estado = ESTADO_TERMINADO; // Consideramos "vacio" a TERMINADO por ahora
        tablaProcesos[i].ticsDormido = 0;
    }
}

// Retorna el indice disponible o -1 si ya llegamos a los 20 maximmos
int buscarEspacioLibreBCP() {
    if (numProcesosActivos >= MAX_PROCESOS) return -1;
    
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].id == -1 || tablaProcesos[i].estado == ESTADO_TERMINADO) {
            return i;
        }
    }
    return -1;
}

// Pide memoria para el PCB (teorica porque ya esta asignada en la tabla)
// Crea el proceso en estado listoy devuelve su ID
int crearProceso(const char *nombrePrograma, int dirBase, int dirLimite, int pcInicial) {
    if (numProcesosActivos >= MAX_PROCESOS) {
        printf("[ERROR SO] No hay mas espacio en la tabla de procesos (Maximo estricto 20).\n");
        return -1;
    }
    
    int indiceLibre = buscarEspacioLibreBCP();
    if(indiceLibre == -1) return -1; // Seguridad
    
    BCP *nuevoP = &tablaProcesos[indiceLibre];
    
    nuevoP->id = contadorIdProcesos++;
    strncpy(nuevoP->nombre, nombrePrograma, MAX_NOMBRE_PROGRAMA - 1);
    nuevoP->nombre[MAX_NOMBRE_PROGRAMA - 1] = '\0'; // Asegurar que sea str de C valido
    
    nuevoP->direccionBase = dirBase;
    nuevoP->direccionLimite = dirLimite;
    
    // Configurar contexto inicial basico para que empiece a correr como si estuviera despertandose
    nuevoP->contexto.psw.modoOperacion = MODO_USUARIO;
    nuevoP->contexto.psw.habilitarInterrupciones = INT_HABILITADAS; // Debe empezar asumiendo ints activas
    nuevoP->contexto.psw.codigoCondicion = CC_CERO; // EVITA QUE BASURA DE MEMORIA DESBORDE LA CODIFICACION LIMITADA DE PSW
    nuevoP->contexto.psw.pc = pcInicial; 
    nuevoP->contexto.rb = dirBase;
    nuevoP->contexto.rl = dirLimite;
    // Nueva Pila por proceso
    nuevoP->contexto.rx = dirLimite; // Pila inactiva hasta dirLimite
    nuevoP->contexto.sp = dirLimite; // SP empieza alli y va bajando

    // Todo proceso empieza logicamente en NUEVO y seguidamente a LISTO según teoría clasica de S.O
    nuevoP->estado = ESTADO_NUEVO;
    
    // LOG OBLIGATORIO - REGLA 11 DE LAS INSTRUCCIONES
    // "Los cambios de estado deben ser consistentes y registrarse obligatoriamente en un archivo log."
    logSistema("Proceso [%s] (ID: %d) creado -> Estado NUEVO", nuevoP->nombre, nuevoP->id);
    
    numProcesosActivos++;
    
    // Inmediatamente lo pasamos a LISTO
    cambiarEstadoProceso(nuevoP->id, ESTADO_LISTO);
    
    return nuevoP->id;
}

// Devuelve un array char para imprimir humanamente al log
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

// Cambiar estado e imprimir un Log del Sistema
void cambiarEstadoProceso(int idProceso, int nuevoEstado) {
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].id == idProceso) {
            
            // Si el proceso ya esta liquidado, no dejas revivir
            if (tablaProcesos[i].estado == ESTADO_TERMINADO) return;
            
            int viejoEstado = tablaProcesos[i].estado;
            tablaProcesos[i].estado = nuevoEstado;
            
            // "Los cambios de estado deben registrarse OBLIGATORIAMENTE en un archivo log."
            logSistema("Proceso (ID: %d) cambio estado %s -> %s", 
                       idProceso, nombreDelEstado(viejoEstado), nombreDelEstado(nuevoEstado));
                       
            if (nuevoEstado == ESTADO_TERMINADO) {
                numProcesosActivos--;
                // Si el SO muere aca se limpia, pero por ahora solo le damos estado Termina
            }
            return;
        }
    }
}

// Vacia un BCP y lo deja para un futuro proceso (aunque sus archivos existiran en RAM hasta limpiarse)
void destruirProceso(int idProceso) {
    cambiarEstadoProceso(idProceso, ESTADO_TERMINADO);
}

// Tick global del SO para despertar procesos despues de su Syscall 4
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

// Analiza si existen hilos en cola o dormidos, o si todo murio.
int hayProcesosVivos() {
    for (int i = 0; i < MAX_PROCESOS; i++) {
        // En este OS con tablas estaticas los vacios no tienen estado -1 sino inicializados en algo extra.
        // Pero `id` al inicio se asignan incrementalmente. NumProcesosActivos dicta el limite creado de hecho:
        if (i < numProcesosActivos) {
            if (tablaProcesos[i].estado != ESTADO_TERMINADO) return 1;
        }
    }
    return 0;
}

// -------------------------------------------------------------
// PLANIFICADOR (POLITICA: Round Robin)
// Decide estrictamente cual proceso le toca. Independiente de como se intercambia.
// Retorna -1 si no hay ninguno listo
// -------------------------------------------------------------
int planificarSiguienteProceso() {
    int idCandidato = -1;
    int inicioBusqueda = 0; // Por defecto empezamos desde el principio de la tabla

    // Si ya hay alguien corriendo, buscamos el siguiente empezando desde donde esta
    if (procesoEnEjecucion != -1) {
        for (int i = 0; i < MAX_PROCESOS; i++) {
            if (tablaProcesos[i].id == procesoEnEjecucion) {
                inicioBusqueda = (i + 1) % MAX_PROCESOS; // Circularidad (+1 mod 20)
                break;
            }
        }
    }

    // Damos una vuelta completa buscando el proximo que este LISTO
    for (int j = 0; j < MAX_PROCESOS; j++) {
        int indiceEvaluar = (inicioBusqueda + j) % MAX_PROCESOS; // circularidad
        
        if (tablaProcesos[indiceEvaluar].id != -1 && 
            tablaProcesos[indiceEvaluar].estado == ESTADO_LISTO) {
            idCandidato = tablaProcesos[indiceEvaluar].id;
            break;
        }
    }
    
    // Si no encontro a ningun otro proceso LISTO, pero el actual sigue EN_EJECUCION,
    // significa que es el unico proceso en el sistema y deberia seguir corriendo el mismo
    // a menos que este terminando o durmiendo.
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

// -------------------------------------------------------------
// DESPACHADOR (MECANISMO)
// Saca de la CPU a quien este corriendo, guarda contexto y mete al nuevo.
// -------------------------------------------------------------
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

                    // --- EXTRACCION DE ESTADO REAL ---
                    // manejarInterrupcion() hizo guardarContexto() apilando 6 words en RAM local.
                    // Para llevarlo al PCB y limpiar la CPU, desapilamos logicamente como si fueramos
                    // el CPU de nuevo, para que el Proceso Saliente quede intacto en su PCB.
                    
                    decodificarPsw(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
                    int spOriginal = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
                    registrosCpu.rx = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
                    registrosCpu.rl = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
                    registrosCpu.rb = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
                    registrosCpu.ac = leerMemoria(registrosCpu.sp); registrosCpu.sp++;
                    
                    // Asegurar SP al original para ese proceso antes del Crash de interrupcion
                    registrosCpu.sp = spOriginal;
                    
                    // Guardamos contexto limpio en BCP ahora que desempaquetamos la Interrupcion
                    tablaProcesos[i].contexto = registrosCpu;
                }
                break;
            }
        }
    }

    // 2. Si nos piden despachar "nadie", solo la CPU queda ociosa.
    if (idNuevoProceso == -1) {
        procesoEnEjecucion = -1;
        // La simulacion frenara o hara NOPs
        return;
    }

    // 3. Montar al nuevo proceso
    int indiceNuevo = -1;
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].id == idNuevoProceso) {
            indiceNuevo = i;
            break;
        }
    }

    if (indiceNuevo != -1) {
        // Enviar a ejecucion
        cambiarEstadoProceso(idNuevoProceso, ESTADO_EJECUCION);
        
        // Cargar todo su contexto personal a los `registrosCpu` (la maquina viva)
        registrosCpu = tablaProcesos[indiceNuevo].contexto;
        
        // --- Limpieza vital de estado de simulador ---
        // Evita que un timer o error pase a heredarse al nuevo proceso
        extern int interrupcionPendiente;
        extern int interrupcionesPendientes[NUM_INTERRUPCIONES];
        extern int contadorCiclos;
        extern void reiniciarDeteccionBucle();
        
        interrupcionPendiente = 0;
        for(int i=0; i<NUM_INTERRUPCIONES; i++) interrupcionesPendientes[i] = 0;
        contadorCiclos = 0;
        reiniciarDeteccionBucle();
        
        // Actualizamos variable de control
        procesoEnEjecucion = idNuevoProceso;

        // "El archivo log debe registrar CADA vez que se agota el quantum, 
        // detallando el ID del proceso saliente y del proceso entrante" (REGLA INSTRUCCIONES)
        if (indiceViejo != -1 && tablaProcesos[indiceViejo].id != idNuevoProceso && tablaProcesos[indiceViejo].estado == ESTADO_LISTO) {
            logSistema(">>> FIN QUANTUM: Saliente Proceso ID %d | Entrante Proceso ID %d <<<", 
                       tablaProcesos[indiceViejo].id, idNuevoProceso);
        } else {
             // Por si el CPU estaba vacio o el saliente murio (TERMINO/DORMIDO) no fue por Quantum, fue por ceder CPU
             logSistema(">>> DESPACHO: Entrante Proceso ID %d <<<", idNuevoProceso);
        }
    }
}

// ============================================
// Funciones Diagnostico (Comandos Fase 2)
// ============================================

void mostrarTablaProcesos() {
    printf("\n=== TABLA DE PROCESOS (ps) ===\n");
    printf("ID\t| ESTADO\t| USO MEMORIA\t| NOMBRE PROG\n");
    printf("------------------------------------------------------\n");
    
    int procesosMostrados = 0;
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].id != -1 && tablaProcesos[i].estado != ESTADO_TERMINADO) {
            char descEstado[20];
            switch (tablaProcesos[i].estado) {
                case ESTADO_NUEVO: strcpy(descEstado, "NUEVO"); break;
                case ESTADO_LISTO: strcpy(descEstado, "LISTO"); break;
                case ESTADO_EJECUCION: strcpy(descEstado, "EN_EJECUCION"); break;
                case ESTADO_DORMIDO: strcpy(descEstado, "DORMIDO"); break;
                default: strcpy(descEstado, "DESCONOCIDO"); break;
            }
            
            int tamanoProceso = tablaProcesos[i].direccionLimite - tablaProcesos[i].direccionBase;
            float porcMem = ((float)tamanoProceso / TAMANO_MEMORIA) * 100.0f;
            
            printf("%d\t| %s\t| %.2f%%\t| %s\n", 
                   tablaProcesos[i].id, 
                   descEstado, 
                   porcMem, 
                   tablaProcesos[i].nombre);
            procesosMostrados++;
        }
    }
    
    if (procesosMostrados == 0) {
        printf("No hay procesos activos en el sistema.\n");
    }
    printf("==============================\n\n");
}
