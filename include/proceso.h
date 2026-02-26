#ifndef PROCESO_H
#define PROCESO_H

#include "hardware.h"

#define MAX_PROCESOS 20

typedef enum {
    ESTADO_NUEVO = 0,
    ESTADO_LISTO = 1,
    ESTADO_EJECUCION = 2,
    ESTADO_DORMIDO = 3,
    ESTADO_TERMINADO = 4
} EstadoProceso;

typedef struct {
    int pid;
    int activo;
    EstadoProceso estado;
    Registros contextoCpu;
    int rb;
    int rl;
    int ticksDormido;
    int codigoSalida;
    char nombrePrograma[50];
} Bcp;

void inicializarGestorProcesos();
int crearProceso(const char *nombrePrograma, int rb, int rl, int pcInicial);
int cambiarEstadoProceso(int pid, EstadoProceso nuevoEstado, const char *motivo);
int actualizarContextoProceso(int pid, const Registros *contexto);
Bcp *obtenerProceso(int pid);
const char *nombreEstadoProceso(EstadoProceso estado);
int cantidadProcesosActivos();

#endif
