// ARCHIVO: hardware.h
// Aca definimos los ladrillos basicos de la maquina virtual. 
// Structures, constantes y todo lo que comparten los componentes.

#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>

// --- Definiciones de Arquitectura ---

// Usamos una estructura para la Palabra porque necesitamos separar signo y magnitud.
// Con un int normal seria un lio manejar el formato decimal de 8 digitos que pidieron.
// Asi controlo facil si es positivo/negativo sin pelearme con bits complementarios.
typedef struct {
    int signo;      // 0 es +, 1 es - (facil para imprimir)
    int digitos;    // Aqui guardamos el valor absoluto (ej: 9999999)
} Palabra;

// El PSW (Processor Status Word) lo empaquetamos en un struct para no tener
// variables sueltas. Es mas facil pasarlo todo junto al guardar contexto.
typedef struct {
    int codigoCondicion;        // Para saber el resultado de la ultima comparacion (<, >, =)
    int modoOperacion;          // Vital: Usuario (0) vs Kernel (1). Seguridad ante todo.
    int habilitarInterrupciones;// Si vale 0, el CPU ignora todo hasta terminar lo critico.
    int pc;                     // El famoso Program Counter.
} Psw;

// El registro de instrucciones (IR) tambien va separado para decodificar facil
// el opcode del modo de direccionamiento. Si fuera un solo int, tendria que hacer
// divisiones y modulos cada vez que quiero saber el opcode.
typedef struct {
    int codigoOperacion;    
    int direccionamiento;   
    int valor;              
} RegistroIr;

// Agrupamos TODOS los registros en una struct para simular el hardware real.
// Ademas, nos ayuda un monton cuando hay que hacer context switch: copias esta
// struct entera y listo, estado guardado.
typedef struct {
    Palabra ac;     // Acumulador: el caballo de batalla, todo pasa por aqui.
    Palabra mar;    // MAR: "Quiero acceder a ESTA direccion"
    Palabra mdr;    // MDR: "Este es el dato que lei/escribire"
    RegistroIr ir;  
    int rb;         // Base: Donde empieza el programa. Proteccion basica.
    int rl;         // Limite: Cuanto mide. Si te pasas -> Sejmet (Violacion de Segmento simulada)
    int rx;         // Para recorrer arrays sin volverse loco con aritmetica de punteros
    int sp;         
    Psw psw;        
} Registros;


// --- Constantes del Sistema ---

// 2000 palabras parece poco, pero para simular basta.
// Las primeras 300 son "terra incognita" para el usuario (reservadas SO).
#define TAMANO_MEMORIA          2000    
#define TAMANO_MEMORIA_SO       300     
#define INICIO_MEMORIA_USUARIO  300     

// Modos de operacion: Flags simples para los ifs de seguridad en memoria.c
#define MODO_USUARIO            0       
#define MODO_KERNEL             1       

// Resultados de comparaciones (AC vs M[x])
// Uso defines en lugar de nums magicos para que el codigo en cpu.c se lea solo.
#define CC_CERO                 0       
#define CC_NEGATIVO             1       
#define CC_POSITIVO             2       
#define CC_DESBORDAMIENTO       3       

// Estados de interrupcion
#define INT_DESHABILITADAS      0       
#define INT_HABILITADAS         1       

// Modos de direccionamiento
// 0: La direccion es donde esta el dato
// 1: La direccion ES el dato (constante)
// 2: La direccion base + indice RX (ideal para arrays)
#define DIR_DIRECTO             0       
#define DIR_INMEDIATO           1       
#define DIR_INDEXADO            2       

// --- Set de Instrucciones (OpCodes) ---
// Estan agrupados por funcionalidad. Uso nombres cortos tipo ensamblador.

// Aritmetica basica (todo contra el Acumulador)
#define OP_SUM                  0       
#define OP_RES                  1       
#define OP_MULT                 2       
#define OP_DIVI                 3       

// Mover datos (RAM <-> AC)
#define OP_LOAD                 4       
#define OP_STR                  5       

// Registros especiales (para no perder el valor de RX)
#define OP_LOADRX               6       
#define OP_STRRX                7       

// Saltos condicionales
// "Si lo ultimo que compare dio X, salta a Y"
#define OP_COMP                 8       
#define OP_JMPE                 9       // Jump Equal
#define OP_JMPNE                10      // Jump Not Equal
#define OP_JMPLT                11      // Jump Less Than
#define OP_JMPLGT               12      // Jump Greater Than (typo mio? deberia ser GT)

// Control de flujo y sistema
#define OP_SVC                  13      // El usuario pide ayuda al SO
#define OP_RETRN                14      
#define OP_HAB                  15      
#define OP_DHAB                 16      
#define OP_TTI                  17      
#define OP_CHMOD                18      

// Gestion de registros protegidos (Solo Kernel deberia tocar esto)
#define OP_LOADRB               19      
#define OP_STRRB                20      
#define OP_LOADRL               21      
#define OP_STRRL                22      
#define OP_LOADSP               23      
#define OP_STRSP                24      

// Pila (Stack)
#define OP_PSH                  25      
#define OP_POP                  26      

// El salto de fe (incondicional)
#define OP_J                    27      

// Instrucciones para manejar el DMA (E/S)
// El CPU configura estos registros y luego dispara el DMA
#define OP_SDMAP                28      
#define OP_SDMAC                29      
#define OP_SDMAS                30      
#define OP_SDMAIO               31      
#define OP_SDMAM                32      
#define OP_SDMAON               33      // "Go DMA!"

// --- Vector de Interrupciones ---
// Lista de posibles catastrofes o eventos.
// Las fatales matan el programa, las otras se manejan y seguimos.

#define INT_SYSCALL_INVALIDA    0       
#define INT_CODIGO_INVALIDO     1       // Opcode desconocido (instruccion basura)
#define INT_SVC                 2       // Software Interrupt (buena)
#define INT_TIMER               3       // Timer hardware (multitarea simulada)
#define INT_IO_DONE             4       // DMA aviso que termino
#define INT_INSTRUCCION_INVALIDA 5      // Usuario intento ejecutar instruccion privilegiada
#define INT_DIRECCION_INVALIDA  6       // Segfault
#define INT_UNDERFLOW           7       // Pop en pila vacia
#define INT_OVERFLOW            8       // Suma dio > 99999999

// --- Definiciones de Disco ---

#define DISCO_CILINDROS         10      
#define DISCO_PISTAS            10      
#define DISCO_SECTORES          100     
#define TAMANO_SECTOR           9       // Cabe justo una Palabra (8 digs + signo?) o char string.

typedef struct {
    char datos[TAMANO_SECTOR];  
} Sector;

// 3D array para simular la geometria fisica del disco magnetico.
// Se accede por [Cil][Pista][Sec]
typedef struct {
    Sector sectores[DISCO_CILINDROS][DISCO_PISTAS][DISCO_SECTORES];
} DiscoDuro;

// El cerebro del DMA.
// Guarda "que" queremos copiar, "de donde" y "a donde".
// Es como un CPU tonto dedicado a copiar memoria.
typedef struct {
    int pistaSeleccionada;      
    int cilindroSeleccionado;   
    int sectorSeleccionado;     
    int direccionIo;            // 0: Disco->RAM, 1: RAM->Disco
    int direccionMemoria;       
    
    int estado;                 // 0 OK, 1 Fallo
    int ocupado;                // Semaforo logico para no pisar transferencias
    
    pthread_t hiloId;           // Para que corra en background real (threads POSIX)
} ControladorDma;


// --- Variables Globales ---
// (Externs para que todos los .c vean la misma instancia del hardware)

extern Registros registrosCpu;
extern int interrupcionPendienteDma;
extern DiscoDuro discoDuro;
extern ControladorDma dma;
extern int cpuEjecutando;

// --- Prototipos de Hardware ---

void inicializarMemoria();      
void inicializarDisco();        

void escribirMemoria(int direccion, Palabra dato);  
Palabra leerMemoria(int direccion);                  

int cicloCpu();                 
int palabraAEntero(Palabra p);  
Palabra enteroAPalabra(int val);

void iniciarTransferenciaDma(); 
int verificarInterrupcionDma(); 
int leerSectorDisco(int pista, int cilindro, int sector, char *bufferSalida);
int escribirSectorDisco(int pista, int cilindro, int sector, const char *bufferEntrada);

#endif 