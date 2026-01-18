/* ARCHIVO: hardware.h
 * DESCRIPCION: Definiciones de hardware para la maquina virtual.
 *              Contiene constantes, estructuras y prototipos de funciones.
 *              Basado en las especificaciones del enunciado (prueba.txt).
 */

#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>

// 1. CONSTANTES DE ARQUITECTURA 

/* Estructura de Palabra (8 digitos decimales totales)
 * Formato: [Signo (1 digito)] [Magnitud (7 digitos)]
 * Signo: 0 = Positivo (+), 1 = Negativo (-)
 */
typedef struct {
    int signo;      // Primer digito: 0 = positivo, 1 = negativo
    int digitos;    // 7 digitos restantes (magnitud del valor)
} Palabra;

/* Registro PSW (Palabra de Estado del Programa)
 * Estructura: [CC (1d)] [Modo (1d)] [Int (1d)] [PC (5d)]
 * Segun enunciado seccion 2: Detalle del PSW */
typedef struct {
    int codigoCondicion;        // CC: 0(=), 1(<), 2(>), 3(Desbordamiento)
    int modoOperacion;          // Modo: 0=Usuario, 1=Kernel
    int habilitarInterrupciones;// Int: 0=Deshabilitadas, 1=Habilitadas
    int pc;                     // PC: Direccion siguiente instruccion (5 digitos)
} Psw;

/* Formato de Instruccion (8 digitos)
 * Estructura: [Codigo Operacion (2d)] [Direccionamiento (1d)] [Valor (5d)]
 * Segun enunciado seccion 3: Formato de Instruccion */
typedef struct {
    int codigoOperacion;    // 2 digitos: codigo de la operacion
    int direccionamiento;   // 1 digito: 0=Directo, 1=Inmediato, 2=Indexado
    int valor;              // 5 digitos: direccion o valor
} RegistroIr;

/* Registros del Procesador
 * Segun enunciado seccion 2: Registros del Procesador */
typedef struct {
    Palabra ac;     // Registro Acumulador (para operaciones aritmeticas)
    Palabra mar;    // Memory Address Register (direccion a buscar)
    Palabra mdr;    // Memory Data Register (dato donde apunta MAR)
    RegistroIr ir;  // Instruction Register (instruccion actual)
    int rb;         // Registro Base (proteccion de memoria)
    int rl;         // Registro Limite (proteccion de memoria)
    int rx;         // Registro base de la pila
    int sp;         // Stack Pointer (puntero al tope de pila)
    Psw psw;        // Palabra de Estado del Sistema
} Registros;

/* CONSTANTES DE MEMORIA RAM
 * Segun enunciado seccion 2: Memoria RAM
 * - Arreglo de 2000 posiciones
 * - Primeras 300 reservadas para el Sistema Operativo */
#define TAMANO_MEMORIA          2000    // Capacidad total: 2000 palabras
#define TAMANO_MEMORIA_SO       300     // 0000 - 0299: Area del Sistema Operativo
#define INICIO_MEMORIA_USUARIO  300     // 0300 - 1999: Espacio de Usuario

/* MODOS DE EJECUCION
 * Segun enunciado seccion 2: Modo de operacion (1 digito)
 */
#define MODO_USUARIO            0       // Modo usuario (sin privilegios)
#define MODO_KERNEL             1       // Modo kernel/privilegiado

/* CODIGOS DE CONDICION (CC)
 * Segun enunciado seccion 2: Codigo de condicion (1 digito)
*/
#define CC_CERO                 0       // Igual a cero (X == Y)
#define CC_NEGATIVO             1       // Menor que cero (X < Y)
#define CC_POSITIVO             2       // Mayor que cero (X > Y)
#define CC_DESBORDAMIENTO       3       // Overflow ocurrido

/* ESTADO DE INTERRUPCIONES
 * Segun enunciado seccion 2: Interrupciones (1 digito)
 */
#define INT_DESHABILITADAS      0       // Interrupciones deshabilitadas
#define INT_HABILITADAS         1       // Interrupciones habilitadas

/* MODOS DE DIRECCIONAMIENTO
 * Segun enunciado seccion 3: Direccionamiento (1 digito)
 */
#define DIR_DIRECTO             0       // Los 5 ultimos digitos referencian memoria
#define DIR_INMEDIATO           1       // Los 5 ultimos digitos son el dato
#define DIR_INDEXADO            2       // Los 5 digitos son indice desde AC

/* CODIGOS DE OPERACION (Set de Instrucciones)
 * Segun enunciado seccion 3: Conjunto de Instrucciones Detallado */

/* Grupo 1: Instrucciones Aritmeticas (Operan sobre AC) */
#define OP_SUM                  0       // Suma: AC = AC + dato
#define OP_RES                  1       // Resta: AC = AC - dato
#define OP_MULT                 2       // Multiplicacion: AC = AC * dato
#define OP_DIVI                 3       // Division: AC = AC / dato

/* Grupo 2: Transferencia de Datos entre AC y Memoria */
#define OP_LOAD                 4       // Carga: AC = M[direccion]
#define OP_STR                  5       // Almacena: M[direccion] = AC

/* Grupo 3: Transferencia entre AC y Registros Especiales */
#define OP_LOADRX               6       // Carga RX en AC
#define OP_STRRX                7       // Almacena AC en RX

/* Grupo 4: Comparacion y Saltos Condicionales */
#define OP_COMP                 8       // Compara dato con AC
#define OP_JMPE                 9       // Salta si igual (AC == M[SP])
#define OP_JMPNE                10      // Salta si no igual (AC != M[SP])
#define OP_JMPLT                11      // Salta si menor que (AC < M[SP])
#define OP_JMPLGT               12      // Salta si mayor que (AC > M[SP])

/* Grupo 5: Control del Sistema y Llamadas */
#define OP_SVC                  13      // Llamada al sistema (syscall)
#define OP_RETRN                14      // Retorno de subrutina
#define OP_HAB                  15      // Habilita interrupciones
#define OP_DHAB                 16      // Deshabilita interrupciones
#define OP_TTI                  17      // Establece tiempo del reloj
#define OP_CHMOD                18      // Cambia modo de ejecucion

/* Grupo 6: Gestion de Registros Base, Limite y Pila */
#define OP_LOADRB               19      // Carga RB en AC
#define OP_STRRB                20      // Almacena AC en RB
#define OP_LOADRL               21      // Carga RL en AC
#define OP_STRRL                22      // Almacena AC en RL
#define OP_LOADSP               23      // Carga SP en AC
#define OP_STRSP                24      // Almacena AC en SP

/* Grupo 7: Operaciones de Pila */
#define OP_PSH                  25      // Apilar: SP--, M[SP] = AC
#define OP_POP                  26      // Desapilar: AC = M[SP], SP++

/* Grupo 8: Salto Incondicional */
#define OP_J                    27      // Salto incondicional: PC = direccion

/* Grupo 9: Instrucciones de E/S (Comunicacion con DMA) */
#define OP_SDMAP                28      // Establece pista del DMA
#define OP_SDMAC                29      // Establece cilindro del DMA
#define OP_SDMAS                30      // Establece sector del DMA
#define OP_SDMAIO               31      // Establece direccion de E/S
#define OP_SDMAM                32      // Establece direccion de memoria
#define OP_SDMAON               33      // Inicia transferencia DMA

/* VECTOR DE INTERRUPCIONES (Codigos 0-8)
 * Segun enunciado seccion 5: Codigos de Interrupcion */
#define INT_SYSCALL_INVALIDA    0       // Codigo de llamada al sistema invalido
#define INT_CODIGO_INVALIDO     1       // Codigo de interrupcion invalido
#define INT_SVC                 2       // Llamada al sistema (svc)
#define INT_TIMER               3       // Interrupcion de reloj
#define INT_IO_DONE             4       // Finalizacion de operacion E/S
#define INT_INSTRUCCION_INVALIDA 5      // Instruccion invalida
#define INT_DIRECCION_INVALIDA  6       // Direccionamiento invalido
#define INT_UNDERFLOW           7       // Underflow
#define INT_OVERFLOW            8       // Overflow

/* ESTRUCTURAS DE E/S (Disco y DMA)
 * Segun enunciado seccion 4: Manejo de DMA y Disco
 * - Minimo 10 pistas, 10 cilindros, 100 sectores/cilindro
 * - Cada sector almacena 9 caracteres */
#define DISCO_CILINDROS         10      // Numero de cilindros
#define DISCO_PISTAS            10      // Numero de pistas
#define DISCO_SECTORES          100     // Sectores por cilindro
#define TAMANO_SECTOR           9       // Caracteres por sector

/* Estructura de un Sector del disco */
typedef struct {
    char datos[TAMANO_SECTOR];  // Contenido del sector (9 caracteres)
} Sector;

/* Estructura del Disco Magnetico
 * Arreglo 3D: [Cilindro][Pista][Sector] */
typedef struct {
    Sector sectores[DISCO_CILINDROS][DISCO_PISTAS][DISCO_SECTORES];
} DiscoDuro;

/* Controlador DMA (Acceso Directo a Memoria)
 * Segun enunciado seccion 4: Manejo de DMA */
typedef struct {
    // Registros de Control para seleccionar ubicacion en disco
    int pistaSeleccionada;      // Pista seleccionada
    int cilindroSeleccionado;   // Cilindro seleccionado
    int sectorSeleccionado;     // Sector seleccionado
    int direccionIo;            // 0 = Leer de disco, 1 = Escribir a disco
    int direccionMemoria;       // Direccion RAM para la transferencia
    
    // Estado del DMA
    int estado;                 // 0 = Exito, 1 = Error
    int ocupado;                // 1 = Operacion en curso, 0 = Libre
    
    // Hilo para operacion asincrona (transferencia en paralelo)
    pthread_t hiloId;
} ControladorDma;

/* VARIABLES GLOBALES (Componentes de Hardware)
 * Declaradas como extern, se definen en el archivo .c correspondiente
 * NOTA: La memoria principal (memoriaPrincipal) y el semaforo del bus
 *       (bloqueoBus) se declaran en memoria.h */

// Registros de la CPU
extern Registros registrosCpu;

// Bandera de interrupcion pendiente del DMA
extern int interrupcionPendienteDma;

// Disco Duro
extern DiscoDuro discoDuro;

// Controlador DMA
extern ControladorDma dma;

// Fin de programa: controlado por los registros RB/RL (RL inclusivo).
// Los defines y el uso de centinela fueron eliminados; el loader ya no escribe centinelas.

// Bandera para saber si la CPU sigue ejecutando
extern int cpuEjecutando;

/* 6. API DE HARDWARE (Prototipos de Funciones)
 * Todas las funciones usan camelCase */

// Inicializacion y finalizacion del hardware
void inicializarHardware();     // Inicializa todos los componentes
void finalizarHardware();       // Libera recursos del hardware
void inicializarMemoria();      // Inicializa la memoria RAM
void inicializarDisco();        // Inicializa el disco duro
void guardarDisco();            // Guarda el contenido del disco

// Operaciones de Memoria
void escribirMemoria(int direccion, Palabra dato);  // Escribe en memoria
Palabra leerMemoria(int direccion);                  // Lee de memoria

// Operaciones de CPU
int cicloCpu();                 // Ejecuta ciclo fetch-decode-execute, retorna 1 para continuar
void reiniciarCpu();            // Reinicia los registros de la CPU
int palabraAEntero(Palabra p);  // Convierte Palabra a entero
Palabra enteroAPalabra(int val);// Convierte entero a Palabra

// Operaciones de Disco y DMA
void iniciarTransferenciaDma(); // Inicia transferencia DMA
int verificarInterrupcionDma(); // Verifica si hay interrupcion del DMA
int leerSectorDisco(int pista, int cilindro, int sector, char *bufferSalida);
int escribirSectorDisco(int pista, int cilindro, int sector, const char *bufferEntrada);

#endif
