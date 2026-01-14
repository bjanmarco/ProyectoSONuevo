/*
 * ============================================================================
 * ARCHIVO: cpu.h
 * DESCRIPCION: Cabecera del componente CPU de la maquina virtual.
 * ============================================================================
 */

#ifndef CPU_H
#define CPU_H

#include "hardware.h"

/* Offset para traduccion de direcciones logicas a fisicas */
#define OFFSET_MEMORIA_USUARIO  300
#define NUM_INTERRUPCIONES      9

/* Estructura para guardar contexto (usado en interrupciones) */
/* NOTA MULTIPROGRAMACION: Esto formaria parte del PCB en un sistema real */
typedef struct {
    Palabra ac;      // Acumulador
    Palabra mar;     // MAR al momento de la interrupcion
    Palabra mdr;     // MDR al momento de la interrupcion
    RegistroIr ir;   // IR al momento de la interrupcion
    int rb;
    int rl;
    int rx;
    int sp;
    Psw psw;
} ContextoCpu;

/* Variables externas del CPU */
extern int cpuEjecutando;
extern int contadorCiclos;
extern int intervaloReloj;
extern int interrupcionPendiente;
extern int codigoInterrupcionPendiente;

/* Funciones principales del CPU */
void inicializarCpu();
void ejecutarCpu();
int cicloCpu();

/* Funciones del ciclo de instruccion */
void faseFetch();
void faseDecode();
int faseExecute();

/* Funciones de manejo de interrupciones */
int manejarInterrupcion(int codigoInterrupcion);
void guardarContexto(ContextoCpu *contexto);
void restaurarContexto(ContextoCpu *contexto);
int verificarInterrupcionesPendientes();

/* Funciones auxiliares */
int traducirDireccion(int direccionLogica);
int verificarProteccionMemoria(int direccionFisica);
int verificarDireccionSalto(int direccionLogica);
int esInstruccionPrivilegiada(int opcode);
Palabra obtenerOperando(int modo, int valor);
void actualizarCodigoCondicion(int resultado);
int palabraAEntero(Palabra p);
Palabra enteroAPalabra(int val);
void imprimirEstadoCpu();
void imprimirLog(const char *mensaje);
int esCentinela(Palabra instruccion);

#endif /* CPU_H */
