#ifndef CPU_H
#define CPU_H

#include "hardware.h"

// este numero 300 es importante porque es donde empieza la memoria del usuario
// antes de esto (de 0 a 299) esta el SO y no podemos tocarlo.
#define OFFSET_MEMORIA_USUARIO  300
#define NUM_INTERRUPCIONES      9

// estas variables son globales para saber que esta pasando en el CPU en todo momento
extern int cpuEjecutando;      // bandera: 1 ejecutando, 0 detenido
extern int contadorCiclos;
extern int intervaloReloj;     // frecuencia del timer (ciclos)
extern int interrupcionPendiente;
// Arreglo de banderas para permitir el anidamiento de mútiples interrupciones a la vez
extern int interrupcionesPendientes[NUM_INTERRUPCIONES];

// Definición de Tipo para el Vector de Interrupciones
typedef int (*RutinaManejadora)(void);
// Arreglo que hace de Vector Real de Punteros a Función C
extern RutinaManejadora vectorInterrupciones[NUM_INTERRUPCIONES];

// funciones para arrancar y controlar el CPU

void inicializarCpu();
void ejecutarCpu();

// esta funcion hace un solo paso del ciclo de vida de una instruccion
int cicloCpu();

// fases del ciclo de una instruccion
void faseFetch(); // busca la instruccion
void faseDecode(); // entiende que es
int faseExecute(); // la hace


// inicializa el vector de punteros a la funcion recien creados
void inicializarVectorInterrupciones();

// El nuevo manejador procesará y limpiará todas las banderas pendientes jerárquicamente
int procesarInterrupcionesPendientes();

// guarda el estado actual en la pila (Stack) del sistema
void guardarContexto();

// recupera el estado desde la pila para volver al programa de usuario
void restaurarContexto();


// funciones para manejar el PSW
Palabra codificarPsw();
void decodificarPsw(Palabra pswPalabra);

// funciones auxiliares y de ayuda
// se explican en cpu.c
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
