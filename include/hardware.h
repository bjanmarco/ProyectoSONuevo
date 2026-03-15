// Definicion de hardware y estructuras base del sistema
#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>

// Arquitectura
typedef struct {
    int signo;
    int digitos;
} Palabra;

// Program Status Word (PSW)
typedef struct {
    int codigoCondicion;
    int modoOperacion;
    int habilitarInterrupciones;
    int pc;
} Psw;

// Registro de Instruccion (IR)
typedef struct {
    int codigoOperacion;    
    int direccionamiento;   
    int valor;              
} RegistroIr;

// Registros del Procesador
typedef struct {
    Palabra ac;
    Palabra mar;
    Palabra mdr;
    RegistroIr ir;  
    int rb;
    int rl;
    int rx;
    int sp;
    Psw psw;        
} Registros;


// Constantes de Memoria
#define TAMANO_MEMORIA          2000    
#define TAMANO_MEMORIA_SO       300     
#define INICIO_MEMORIA_USUARIO  300     

// Modos de Operacion
#define MODO_USUARIO            0       
#define MODO_KERNEL             1       

// Codigos de Condicion
#define CC_CERO                 0       
#define CC_NEGATIVO             1       
#define CC_POSITIVO             2       
#define CC_DESBORDAMIENTO       3       

// Estados de Interrupcion
#define INT_DESHABILITADAS      0       
#define INT_HABILITADAS         1       

// Modos de Direccionamiento
#define DIR_DIRECTO             0       
#define DIR_INMEDIATO           1       
#define DIR_INDEXADO            2       

// Set de Instrucciones
// Aritmetica
#define OP_SUM                  0       
#define OP_RES                  1       
#define OP_MULT                 2       
#define OP_DIVI                 3       

// RAM a Acumulador
#define OP_LOAD                 4       
#define OP_STR                  5       

// Manipulacion de Registros Base/Relativos
#define OP_LOADRX               6       
#define OP_STRRX                7       

// Control de Flujo Condicional
#define OP_COMP                 8       
#define OP_JMPE                 9       
#define OP_JMPNE                10      
#define OP_JMPLT                11      
#define OP_JMPLGT               12      

// Sistema y Comandos Privilegiados
#define OP_SVC                  13      
#define OP_RETRN                14      
#define OP_HAB                  15      
#define OP_DHAB                 16      
#define OP_TTI                  17      
#define OP_CHMOD                18      

// Modificacion de Registros Protegidos
#define OP_LOADRB               19      
#define OP_STRRB                20      
#define OP_LOADRL               21      
#define OP_STRRL                22      
#define OP_LOADSP               23      
#define OP_STRSP                24      

// Operaciones de Pila
#define OP_PSH                  25      
#define OP_POP                  26      

// Salto Incondicional
#define OP_J                    27      

// Instrucciones DMA (E/S)
#define OP_SDMAP                28      
#define OP_SDMAC                29      
#define OP_SDMAS                30      
#define OP_SDMAIO               31      
#define OP_SDMAM                32      
#define OP_SDMAON               33      

// Vector de Interrupciones
#define INT_SYSCALL_INVALIDA    0       
#define INT_CODIGO_INVALIDO     1       
#define INT_SVC                 2       
#define INT_TIMER               3       
#define INT_IO_DONE             4       
#define INT_INSTRUCCION_INVALIDA 5      
#define INT_DIRECCION_INVALIDA  6       
#define INT_UNDERFLOW           7       
#define INT_OVERFLOW            8

// Geometria del Disco Duro
#define DISCO_CILINDROS         10      
#define DISCO_PISTAS            10      
#define DISCO_SECTORES          100     
#define TAMANO_SECTOR           9       

typedef struct {
    char datos[TAMANO_SECTOR];  
} Sector;

typedef struct {
    Sector sectores[DISCO_CILINDROS][DISCO_PISTAS][DISCO_SECTORES];
} DiscoDuro;

// Controlador DMA
typedef struct {
    int pistaSeleccionada;      
    int cilindroSeleccionado;   
    int sectorSeleccionado;     
    int direccionIo;            
    int direccionMemoria;       
    
    int estado;                 
    int ocupado;                
    
    pthread_t hiloId;           
} ControladorDma;


// Variables Globales
extern Registros registrosCpu;
extern int interrupcionPendienteDma;
extern DiscoDuro discoDuro;
extern ControladorDma dma;
extern int cpuEjecutando;

// Prototipos

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