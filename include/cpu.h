#ifndef CPU_H
#define CPU_H

#include "hardware.h"

// este numero 300 es importante porque es donde empieza la memoria del usuario
// antes de esto (de 0 a 299) esta el SO y no podemos tocarlo.
#define OFFSET_MEMORIA_USUARIO  300

#define NUM_INTERRUPCIONES      9

// esta estructura es super util para guardar una captura del estado del CPU
// cuando ocurre una interrupcion (como un error o una llamada al sistema)
// necesitamos guardar todos los registros aqui para que al volver
// podamos restaurarlos y seguir ejecutando el programa como si nada hubiera pasado 
typedef struct {
    Palabra ac;      // el acumulador, aqui se hacen las sumas y restas
    Palabra mar;     // la direccion de memoria que estamos apuntando
    Palabra mdr;     // el dato que leimos o vamos a escribir en memoria
    RegistroIr ir;   // la instruccion que estamos ejecutando ahora mismo
    int rb;          // registro base (donde empieza mi programa)
    int rl;          // registro limite (hasta donde llega mi programa)
    int rx;          // registro indice (para recorrer arreglos)
    int sp;          // puntero de pila
    Psw psw;         // palabra de estado (si soy usuario o kernel, flags, etc)
} ContextoCpu;

// estas variables son globales para saber que esta pasando en el CPU en todo momento
extern int cpuEjecutando;      // bandera: 1 ejecutando, 0 detenido
extern int contadorCiclos;
extern int intervaloReloj;     // frecuencia del timer (ciclos)
extern int interrupcionPendiente;
extern int codigoInterrupcionPendiente;

// funciones para arrancar y controlar el CPU

void inicializarCpu();
void ejecutarCpu();

// esta funcion hace un solo paso del ciclo de vida de una instruccion
int cicloCpu();

// fases del ciclo de una instruccion
void faseFetch(); // busca la instruccion
void faseDecode(); // entiende que es
int faseExecute(); // la hace


// esta es la funcion central que decide que hacer cuando ocurre una interrupcion
// recibe el codigo del problema y actua en consecuencia (si es fatal mata el proceso)
int manejarInterrupcion(int codigoInterrupcion);

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
