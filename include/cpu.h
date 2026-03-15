#ifndef CPU_H
#define CPU_H

#include "hardware.h"

// Inicio de memoria para programas (0 a 299 es para SO)
#define OFFSET_MEMORIA_USUARIO  300
#define NUM_INTERRUPCIONES      9

extern int cpuEjecutando;
extern int contadorCiclos;
extern int intervaloReloj;
extern int interrupcionPendiente;
extern int interrupcionesPendientes[NUM_INTERRUPCIONES];

typedef int (*RutinaManejadora)(void);
extern RutinaManejadora vectorInterrupciones[NUM_INTERRUPCIONES];
extern RutinaManejadora vectorInterrupciones[NUM_INTERRUPCIONES];

void inicializarCpu();
void ejecutarCpu();

// Ejecuta una sola instruccion
int cicloCpu();

// Fases del ciclo de instruccion
void faseFetch();
void faseDecode();
int faseExecute();

void inicializarVectorInterrupciones();
int procesarInterrupcionesPendientes();

void guardarContexto();
void restaurarContexto();

Palabra codificarPsw();
void decodificarPsw(Palabra pswPalabra);
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

#endif 
