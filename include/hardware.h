// aca definimos las bases de la maquina virtual. 
// estructuras, constantes y todo lo que comparten los componentes.
#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>

// definiciones de arquitectura

// usamos una estructura para la Palabra porque necesitamos separar signo y magnitud
// con un int normal seria un lio manejar el formato decimal de 8 digitos que pidieron
// asi controlo facil si es positivo/negativo sin pelearme con bits complementarios
typedef struct {
    int signo;      // 0 es +, 1 es - 
    int digitos;    // aqui guardamos el valor absoluto
} Palabra;

// el psw lo empaquetamos en un struct para no tener variables sueltas 
// asi es mas facil pasarlo todo junto al guardar contexto
typedef struct {
    int codigoCondicion;        // para saber el resultado de la ultima comparacion (<, >, =)
    int modoOperacion;          // vital: Usuario (0) vs Kernel (1). seguridad ante todo
    int habilitarInterrupciones;// si vale 0, el CPU ignora todo hasta terminar lo critico
    int pc;                     // contador de programa
} Psw;

// el registro de instrucciones tambien va separado para decodificar facil el opcode del modo de direccionamiento
// si fuera un solo int, tendria que hacer divisiones y modulos cada vez que quiero saber el opcode
typedef struct {
    int codigoOperacion;    
    int direccionamiento;   
    int valor;              
} RegistroIr;

// agrupamos TODOS los registros en una struct para simular el hardware real
// ademas, nos ayuda un monton cuando hay que hacer cambios de contexto copias esta struct entera y listo, estado guardado
typedef struct {
    Palabra ac;     // acumulador 
    Palabra mar;    // indica la  direccion a la que quiero acceder
    Palabra mdr;    // inica que este es el dato que lei/escribire
    RegistroIr ir;  
    int rb;         // donde empieza el programa. Proteccion basica.
    int rl;         // limite: cuanto mide, si nos pasamos violamos el segmento
    int rx;         // para recorrer arreglos sin volverse loco con aritmetica de punteros
    int sp;         
    Psw psw;        
} Registros;


// constantes del sistema

// las primeras 300 son reservadas para el SO
#define TAMANO_MEMORIA          2000    
#define TAMANO_MEMORIA_SO       300     
#define INICIO_MEMORIA_USUARIO  300     

// modos de operacion banderas simples para los ifs de seguridad en memoria.c
#define MODO_USUARIO            0       
#define MODO_KERNEL             1       

// resultados de comparaciones (AC vs M[x])
#define CC_CERO                 0       
#define CC_NEGATIVO             1       
#define CC_POSITIVO             2       
#define CC_DESBORDAMIENTO       3       

// estados de interrupcion
#define INT_DESHABILITADAS      0       
#define INT_HABILITADAS         1       

// modos de direccionamiento
// 0: La direccion es donde esta el dato
// 1: La direccion ES el dato (constante)
// 2: La direccion base + indice RX (ideal para arrays)
#define DIR_DIRECTO             0       
#define DIR_INMEDIATO           1       
#define DIR_INDEXADO            2       

// set de instrucciones 
// agrupados por funcionalidad. uso nombres cortos tipo ensamblador
// aritmetica basica (todo contra el Acumulador)
#define OP_SUM                  0       
#define OP_RES                  1       
#define OP_MULT                 2       
#define OP_DIVI                 3       

// mover datos (RAM <-> AC)
#define OP_LOAD                 4       
#define OP_STR                  5       

// registros especiales (para no perder el valor de RX)
#define OP_LOADRX               6       
#define OP_STRRX                7       

// saltos condicionales
// si lo ultimo que compare dio X, salta a Y
#define OP_COMP                 8       // compara contra ac
#define OP_JMPE                 9       // Salta si AC=M[SP] (iguales)
#define OP_JMPNE                10      // Salta si AC!=M[SP] (distintos)
#define OP_JMPLT                11      // Salta si AC<M[SP] (menor)
#define OP_JMPLGT               12      // Salta si AC>M[SP] (mayor)

// control de flujo y sistema
#define OP_SVC                  13      // el usuario pide ayuda al SO
#define OP_RETRN                14      
#define OP_HAB                  15      
#define OP_DHAB                 16      
#define OP_TTI                  17      
#define OP_CHMOD                18      

// gestion de registros protegidos (solo kernel debe tocar esto)
#define OP_LOADRB               19      
#define OP_STRRB                20      
#define OP_LOADRL               21      
#define OP_STRRL                22      
#define OP_LOADSP               23      
#define OP_STRSP                24      

// pila (stack)
#define OP_PSH                  25      
#define OP_POP                  26      

// salto de fe (incondicional)
#define OP_J                    27      

// instrucciones para manejar el DMA (E/S)
// el CPU configura estos registros y luego dispara el DMA
#define OP_SDMAP                28      
#define OP_SDMAC                29      
#define OP_SDMAS                30      
#define OP_SDMAIO               31      
#define OP_SDMAM                32      
#define OP_SDMAON               33      // "Go DMA!"

// vector de interrupciones
// lista de posibles eventos.
// las fatales matan el programa, las otras se manejan y seguimos

#define INT_SYSCALL_INVALIDA    0       
#define INT_CODIGO_INVALIDO     1       // opcode desconocido (instruccion basura)
#define INT_SVC                 2       // software interrupt (buena)
#define INT_TIMER               3       // timer hardware (multitarea simulada)
#define INT_IO_DONE             4       // DMA aviso que termino
#define INT_INSTRUCCION_INVALIDA 5      // usuario intento ejecutar instruccion privilegiada
#define INT_DIRECCION_INVALIDA  6       // segfault
#define INT_UNDERFLOW           7       // pop en pila vacia
#define INT_OVERFLOW            8       // suma dio > 99999999

// definiciones de disco

#define DISCO_CILINDROS         10      
#define DISCO_PISTAS            10      
#define DISCO_SECTORES          100     
#define TAMANO_SECTOR           9       // cabe justo una Palabra (8 digs + signo?) o char string

typedef struct {
    char datos[TAMANO_SECTOR];  
} Sector;

// arreglo de 3 niveles para simular la geometria fisica del disco magnetico
// se accede por [cil][pista][sec]
typedef struct {
    Sector sectores[DISCO_CILINDROS][DISCO_PISTAS][DISCO_SECTORES];
} DiscoDuro;

// el cerebro del DMA
// guarda que queremos copiar, de donde y a donde
// es como un CPU tonto dedicado a copiar memoria
typedef struct {
    int pistaSeleccionada;      
    int cilindroSeleccionado;   
    int sectorSeleccionado;     
    int direccionIo;            // 0: Disco->RAM, 1: RAM->Disco
    int direccionMemoria;       
    
    int estado;                 // 0 OK, 1 Fallo
    int ocupado;                // semaforo logico para no pisar transferencias
    
    pthread_t hiloId;           // Para que corra en background real (threads POSIX)
} ControladorDma;


// Variables Globales
// externs para que todos los .c vean la misma instancia del hardware

extern Registros registrosCpu;
extern int interrupcionPendienteDma;
extern DiscoDuro discoDuro;
extern ControladorDma dma;
extern int cpuEjecutando;

// Prototipos de Hardware

void inicializarMemoria();       // se explica en su .h
void inicializarDisco();         // se explica en su .h

void escribirMemoria(int direccion, Palabra dato);  // se explica en su .c
Palabra leerMemoria(int direccion);                  // se explica en su .c

int cicloCpu();                 // se explica en su .c
int palabraAEntero(Palabra p);  // se explica en su .c
Palabra enteroAPalabra(int val); // se explica en su .c

void iniciarTransferenciaDma(); // se explica en su .h
int verificarInterrupcionDma(); // se explica en su .h
int leerSectorDisco(int pista, int cilindro, int sector, char *bufferSalida); // se explica en su .c
int escribirSectorDisco(int pista, int cilindro, int sector, const char *bufferEntrada); // se explica en su .c

#endif 