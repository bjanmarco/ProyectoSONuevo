#include <stdio.h>
#include <string.h>
#include "../include/proceso.h"
#include "../include/logger.h"

static Bcp tablaProcesos[MAX_PROCESOS];
static int siguientePid = 1;

static int indicePorPid(int pid) {
    int i;
    for (i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].activo && tablaProcesos[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

static int transicionValida(EstadoProceso origen, EstadoProceso destino) {
    if (origen == destino) return 1;

    switch (origen) {
        case ESTADO_NUEVO:
            return destino == ESTADO_LISTO || destino == ESTADO_TERMINADO;
        case ESTADO_LISTO:
            return destino == ESTADO_EJECUCION || destino == ESTADO_DORMIDO || destino == ESTADO_TERMINADO;
        case ESTADO_EJECUCION:
            return destino == ESTADO_LISTO || destino == ESTADO_DORMIDO || destino == ESTADO_TERMINADO;
        case ESTADO_DORMIDO:
            return destino == ESTADO_LISTO || destino == ESTADO_TERMINADO;
        case ESTADO_TERMINADO:
            return 0;
        default:
            return 0;
    }
}

const char *nombreEstadoProceso(EstadoProceso estado) {
    switch (estado) {
        case ESTADO_NUEVO: return "NUEVO";
        case ESTADO_LISTO: return "LISTO";
        case ESTADO_EJECUCION: return "EN_EJECUCION";
        case ESTADO_DORMIDO: return "DORMIDO";
        case ESTADO_TERMINADO: return "TERMINADO";
        default: return "DESCONOCIDO";
    }
}

void inicializarGestorProcesos() {
    int i;
    memset(tablaProcesos, 0, sizeof(tablaProcesos));
    siguientePid = 1;
    for (i = 0; i < MAX_PROCESOS; i++) {
        tablaProcesos[i].pid = 0;
        tablaProcesos[i].activo = 0;
        tablaProcesos[i].estado = ESTADO_TERMINADO;
        tablaProcesos[i].ticksDormido = 0;
        tablaProcesos[i].codigoSalida = 0;
    }
    logSistema("[PROC] Gestor de procesos inicializado. Capacidad=%d", MAX_PROCESOS);
}

int cantidadProcesosActivos() {
    int i;
    int total = 0;
    for (i = 0; i < MAX_PROCESOS; i++) {
        if (tablaProcesos[i].activo && tablaProcesos[i].estado != ESTADO_TERMINADO) {
            total++;
        }
    }
    return total;
}

int crearProceso(const char *nombrePrograma, int rb, int rl, int pcInicial) {
    int i;

    if (cantidadProcesosActivos() >= MAX_PROCESOS) {
        logSistema("[PROC] ERROR: Limite de %d procesos alcanzado", MAX_PROCESOS);
        return -1;
    }

    for (i = 0; i < MAX_PROCESOS; i++) {
        if (!tablaProcesos[i].activo || tablaProcesos[i].estado == ESTADO_TERMINADO) {
            tablaProcesos[i].pid = siguientePid++;
            tablaProcesos[i].activo = 1;
            tablaProcesos[i].estado = ESTADO_NUEVO;
            tablaProcesos[i].rb = rb;
            tablaProcesos[i].rl = rl;
            tablaProcesos[i].ticksDormido = 0;
            tablaProcesos[i].codigoSalida = 0;
            memset(&tablaProcesos[i].contextoCpu, 0, sizeof(Registros));
            tablaProcesos[i].contextoCpu.rb = rb;
            tablaProcesos[i].contextoCpu.rl = rl;
            tablaProcesos[i].contextoCpu.psw.pc = pcInicial;
            tablaProcesos[i].contextoCpu.psw.modoOperacion = MODO_USUARIO;
            tablaProcesos[i].contextoCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
            tablaProcesos[i].contextoCpu.psw.codigoCondicion = CC_CERO;
            strncpy(tablaProcesos[i].nombrePrograma, nombrePrograma, sizeof(tablaProcesos[i].nombrePrograma) - 1);
            tablaProcesos[i].nombrePrograma[sizeof(tablaProcesos[i].nombrePrograma) - 1] = '\0';

            logSistema("[PROC] PID=%d creado. Programa=%s RB=%d RL=%d Estado=%s",
                       tablaProcesos[i].pid,
                       tablaProcesos[i].nombrePrograma,
                       tablaProcesos[i].rb,
                       tablaProcesos[i].rl,
                       nombreEstadoProceso(tablaProcesos[i].estado));

            return tablaProcesos[i].pid;
        }
    }

    logSistema("[PROC] ERROR: No se encontro entrada libre para nuevo proceso");
    return -1;
}

int cambiarEstadoProceso(int pid, EstadoProceso nuevoEstado, const char *motivo) {
    int i = indicePorPid(pid);
    EstadoProceso anterior;

    if (i < 0) {
        logSistema("[PROC] ERROR: PID=%d no existe para cambio de estado", pid);
        return 1;
    }

    anterior = tablaProcesos[i].estado;
    if (!transicionValida(anterior, nuevoEstado)) {
        logSistema("[PROC] ERROR: Transicion invalida PID=%d %s -> %s",
                   pid,
                   nombreEstadoProceso(anterior),
                   nombreEstadoProceso(nuevoEstado));
        return 1;
    }

    tablaProcesos[i].estado = nuevoEstado;
    logSistema("[PROC] PID=%d Estado %s -> %s Motivo=%s",
               pid,
               nombreEstadoProceso(anterior),
               nombreEstadoProceso(nuevoEstado),
               (motivo != NULL) ? motivo : "N/A");
    return 0;
}

int actualizarContextoProceso(int pid, const Registros *contexto) {
    int i = indicePorPid(pid);

    if (i < 0 || contexto == NULL) {
        return 1;
    }

    tablaProcesos[i].contextoCpu = *contexto;
    logSistema("[PROC] PID=%d Contexto actualizado PC=%d AC=%c%07d",
               pid,
               tablaProcesos[i].contextoCpu.psw.pc,
               tablaProcesos[i].contextoCpu.ac.signo ? '-' : '+',
               tablaProcesos[i].contextoCpu.ac.digitos);
    return 0;
}

Bcp *obtenerProceso(int pid) {
    int i = indicePorPid(pid);
    if (i < 0) return NULL;
    return &tablaProcesos[i];
}
