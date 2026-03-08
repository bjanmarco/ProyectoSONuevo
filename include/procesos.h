#ifndef PROCESOS_H
#define PROCESOS_H

#include "hardware.h"
#include "loader.h"

// estados obligatorios de un proceso
#define ESTADO_NUEVO       0
#define ESTADO_LISTO       1
#define ESTADO_EJECUCION   2
#define ESTADO_DORMIDO     3
#define ESTADO_TERMINADO   4

// Limite estricto de procesos
#define MAX_PROCESOS       20

// Bloque de Control de Proceso (BCP)
typedef struct {
    int id;                             // ID unico del proceso
    int estado;                         // Estado actual (NUEVO, LISTO, etc)
    char nombre[MAX_NOMBRE_PROGRAMA];   // Nombre del programa asociado
    
    // Regiones en memoria
    int direccionBase;                  // Registro Base (RB)
    int direccionLimite;                // Registro Limite (RL)
    
    // Contexto del procesador protegido cuando no esta en CPU
    Registros contexto;
    
    // Datos extra
    int ticsDormido;                    // Contador para la syscall Dormir
} BCP;

// Constantes globales de la tabla de procesos
extern BCP tablaProcesos[MAX_PROCESOS];
extern int numProcesosActivos;
extern int procesoEnEjecucion;          // Guarda el ID del proceso que actualmente tiene la CPU. -1 si ninguno

// Prototipos de funciones
void inicializarGestorProcesos();
int crearProceso(const char *nombrePrograma, int dirBase, int dirLimite, int pcInicial);
void cambiarEstadoProceso(int idProceso, int nuevoEstado);
void destruirProceso(int idProceso);
void actualizarProcesosDormidos();
int hayProcesosVivos();

// Planificador y Despachador (Round Robin)
int planificarSiguienteProceso();
void despacharProceso(int idNuevoProceso);

// Tabla grafica para consola del usuario (ps)
void mostrarTablaProcesos();

#endif
