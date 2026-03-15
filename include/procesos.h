#ifndef PROCESOS_H
#define PROCESOS_H

#include "hardware.h"
#include "loader.h"

#define ESTADO_NUEVO       0
#define ESTADO_LISTO       1
#define ESTADO_EJECUCION   2
#define ESTADO_DORMIDO     3
#define ESTADO_TERMINADO   4

#define MAX_PROCESOS       20

typedef struct {
    int id;
    int estado;
    char nombre[MAX_NOMBRE_PROGRAMA];
    
    int direccionBase;
    int direccionLimite;
    int tamanoCodigo;
    
    Registros contexto;
    
    int ticsDormido;
    int requiereKernel;
} BCP;

extern BCP tablaProcesos[MAX_PROCESOS];
extern int numProcesosActivos;
extern int procesoEnEjecucion;

void inicializarGestorProcesos();
void limpiarProcesosTerminados();
int crearProceso(const char *nombrePrograma, int dirBase, int dirLimite, int pcInicial, int tamanoCodigo, int requiereKernel);
void cambiarEstadoProceso(int idProceso, int nuevoEstado);
void destruirProceso(int idProceso);
void actualizarProcesosDormidos();
int hayProcesosVivos();

int planificarSiguienteProceso();
void despacharProceso(int idNuevoProceso);

void mostrarTablaProcesos();

#endif
