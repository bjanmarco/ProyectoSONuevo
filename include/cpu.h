// ARCHIVO: cpu.h
// Bueno aqui definimos todo lo que tiene que ver con el cerebro de la maquina (el CPU).
// Estan las estructuras para guardar el estado y todas las funciones que necesitamos
// para que el procesador entienda y ejecute las instrucciones.

#ifndef CPU_H
#define CPU_H

#include "hardware.h"

// Este numero 300 es importante porque es donde empieza la memoria del usuario.
// Antes de esto (de 0 a 299) esta el Sistema Operativo y no podemos tocarlo.
#define OFFSET_MEMORIA_USUARIO  300

#define NUM_INTERRUPCIONES      9

// Esta estructura es super util para guardar una "foto" del estado del CPU.
// Cuando ocurre una interrupcion (como un error o una llamada al sistema),
// necesitamos guardar todos los registros aqui para que al volver,
// podamos restaurarlos y seguir ejecutando el programa como si nada hubiera pasado.
typedef struct {
    Palabra ac;      // El acumulador, aqui se hacen las sumas y restas
    Palabra mar;     // La direccion de memoria que estamos apuntando
    Palabra mdr;     // El dato que leimos o vamos a escribir en memoria
    RegistroIr ir;   // La instruccion que estamos ejecutando ahora mismo
    int rb;          // Registro base (donde empieza mi programa)
    int rl;          // Registro limite (hasta donde llega mi programa)
    int rx;          // Registro indice (para recorrer arreglos)
    int sp;          // Puntero de pila (Stack Pointer)
    Psw psw;         // Palabra de estado (si soy usuario o kernel, flags, etc)
} ContextoCpu;

// Estas variables son globales para saber que esta pasando en el CPU en todo momento
extern int cpuEjecutando;      // Bandera: 1 ejecutando, 0 detenido
extern int contadorCiclos;
extern int intervaloReloj;     // Frecuencia del timer (ciclos)
extern int interrupcionPendiente;
extern int codigoInterrupcionPendiente;

// --- Funciones para arrancar y controlar el CPU ---

void inicializarCpu();
void ejecutarCpu();

// Esta funcion hace un solo paso del ciclo de vida de una instruccion.
// Es decir: Busca la instruccion (Fetch), Entiende que es (Decode) y La hace (Execute).
int cicloCpu();

//Fases del ciclo de una instruccion
void faseFetch();
void faseDecode();
int faseExecute();


// Esta es la funcion central que decide que hacer cuando ocurre una interrupcion.
// Recibe el codigo del problema y actua en consecuencia (si es fatal mata el proceso).
int manejarInterrupcion(int codigoInterrupcion);

// Guarda el estado actual en la pila (Stack) del sistema.
void guardarContexto();

// Recupera el estado desde la pila para volver al programa de usuario.
void restaurarContexto();


// --- Funciones para manejar el PSW (Processor Status Word) ---
Palabra codificarPsw();
void decodificarPsw(Palabra pswPalabra);

// --- Funciones Auxiliares y de Ayuda ---
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
