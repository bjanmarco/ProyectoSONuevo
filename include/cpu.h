/* ARCHIVO: cpu.h
 * DESCRIPCION: Cabecera del componente CPU de la maquina virtual.
 */

#ifndef CPU_H
#define CPU_H

#include "hardware.h"

// Offset para traduccion de direcciones logicas a fisicas 
#define OFFSET_MEMORIA_USUARIO  300
#define NUM_INTERRUPCIONES      9

// Estructura para guardar contexto (usado en interrupciones) 
// NOTA MULTIPROGRAMACION: Esto formaria parte del PCB en un sistema real 
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

// Variables externas del CPU 
extern int cpuEjecutando;
extern int contadorCiclos;
extern int intervaloReloj;
extern int interrupcionPendiente;
extern int codigoInterrupcionPendiente;

// Funciones principales del CPU 
// Inicializa los registros y variables globales del CPU 
void inicializarCpu();

// Bucle principal que ejecuta instrucciones hasta detenerse 
void ejecutarCpu();

// Ejecuta un ciclo Fetch-Decode-Execute. Retorna 1 si continua, 0 si para 
int cicloCpu();

// Funciones del ciclo de instruccion 
// Fase 1: Busca la instruccion en memoria y actualiza PC 
void faseFetch();

// Fase 2: Decodifica la instruccion en Opcode, Modo y Valor 
void faseDecode();

// Fase 3: Ejecuta la operacion. Retorna 1 (exito) o 0 (fin/error) 
int faseExecute();

// Funciones de manejo de interrupciones 
// Gestiona interrupciones (Timer, IO, Errores). Retorna 1 si es recuperable 
int manejarInterrupcion(int codigoInterrupcion);

// Guarda el estado actual (PSW, Registros) en la pila del sistema 
void guardarContexto();

// Restoura el estado previo (PSW, Registros) desde la pila 
void restaurarContexto();

// Verifica si hay interrupciones externas pendientes 
int verificarInterrupcionesPendientes();

// Funciones auxiliares para PSW 
// Empaqueta el PSW en una Palabra (para guardarlo en pila) 
Palabra codificarPsw();

// Desempaqueta una Palabra en la estructura PSW 
void decodificarPsw(Palabra pswPalabra);


// Funciones auxiliares 
// Convierte direccion Logica (Usuario) a Fisica (RAM real) 
int traducirDireccion(int direccionLogica);

// Verifica si la direccion fisica esta dentro de los limites RB-RL 
int verificarProteccionMemoria(int direccionFisica);

// Verifica si la direccion de salto es valida dentro del proceso 
int verificarDireccionSalto(int direccionLogica);

// Retorna 1 si el opcode requiere privilegios de Kernel 
int esInstruccionPrivilegiada(int opcode);

// Obtiene el valor del operando segun modo (Directo, Inmediato, Indexado) 
Palabra obtenerOperando(int modo, int valor);

// Actualiza flags CC (Cero, Positivo, Negativo) segun resultado 
void actualizarCodigoCondicion(int resultado);

// Convierte estructura Palabra a int de C 
int palabraAEntero(Palabra p);

// Convierte int de C a estructura Palabra 
Palabra enteroAPalabra(int val);

// Muestra el contenido de todos los registros en consola 
void imprimirEstadoCpu();

// Imprime mensaje con numero de ciclo actual 
void imprimirLog(const char *mensaje);

#endif 
