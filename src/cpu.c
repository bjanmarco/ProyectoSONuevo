// aqui definimos todo lo que tiene que ver con el cerebro de la maquina (el CPU)
// estan las estructuras para guardar el estado y todas las funciones que necesitamos
// para que el procesador entienda y ejecute las instrucciones
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/cpu.h"
#include "../include/hardware.h"
#include "../include/memoria.h"
#include "../include/disco.h"
#include "../include/dma.h"
#include "../include/logger.h"

// bandera que indica si el CPU esta ejecutando
int cpuEjecutando = 0;

// variables para deteccion de bucles infinitos
static int lastPC = -1;
static int repetitionCount = 0;

// funcion para reiniciar la deteccion de bucles
void reiniciarDeteccionBucle() {
    lastPC = -1;
    repetitionCount = 0;
}

// contador de ciclos para interrupciones de reloj
int contadorCiclos = 0;

// intervalo para generar interrupcion de reloj (0 = deshabilitado)
int intervaloReloj = 0;

// bandera de interrupcion pendiente
int interrupcionPendiente = 0;

// codigo de la interrupcion pendiente
int codigoInterrupcionPendiente = -1;

Registros registrosCpu;

int palabraAEntero(Palabra p) {
    int valor = p.digitos;
    if (p.signo == 1) {
        valor = -valor;  // aplicar signo negativo
    }
    return valor;
}

// convierte un entero con signo a una palabra (signo-magnitud)
Palabra enteroAPalabra(int val) {
    Palabra p;
    if (val < 0) {
        p.signo = 1;         // negativo
        p.digitos = -val;    // magnitud absoluta
    } else {
        p.signo = 0;         // positivo
        p.digitos = val;
    }
    // verificar overflow 
    // se requiere 8 digitos para instrucciones como JMP
    if (p.digitos > 99999999) {
        p.digitos = 99999999;
        // actualizar codigo de condicion a desbordamiento
        registrosCpu.psw.codigoCondicion = CC_DESBORDAMIENTO;
        interrupcionPendiente = 1;
        codigoInterrupcionPendiente = INT_OVERFLOW;
    }
    return p;
}

// muestra mensajes en el log
void imprimirLog(const char *mensaje) {
    // se incluye el ciclo actual para contexto
    logCpu("[Ciclo %d] %s", contadorCiclos, mensaje);
}

int traducirDireccion(int direccionLogica) {
    if (registrosCpu.psw.modoOperacion == MODO_KERNEL) {
        return direccionLogica;
    }
    return direccionLogica + registrosCpu.rb;
}

// verifica proteccion de memoria (solo en modo usuario).
// retorna 1 si es valida, 0 si viola proteccion.
int verificarProteccionMemoria(int direccionFisica) {
    // en modo kernel no se verifica proteccion
    if (registrosCpu.psw.modoOperacion == MODO_KERNEL) {
        return 1;
    }
    // en modo usuario RB es menor o igual a direccion_fisica y, direccion_fisica es menor o igual a RL
    if (direccionFisica >= registrosCpu.rb && direccionFisica <= registrosCpu.rl) {
        return 1;
    }
    return 0;
}

int esInstruccionPrivilegiada(int opcode) {
    // instrucciones de usuario
    if (opcode >= 0 && opcode <= 5) return 0;   // aritmeticas y transferencia
    if (opcode >= 8 && opcode <= 13) return 0;  // comparacion, saltos, SVC
    if (opcode >= 25 && opcode <= 27) return 0; // pila y salto incondicional
    // todo lo demas es privilegiado
    return 1;
}

int verificarDireccionSalto(int direccionLogica) {
    int direccionFisica;
    
    // en modo kernel no se verifica
    if (registrosCpu.psw.modoOperacion == MODO_KERNEL) {
        return 1; 
    }
    
    // traducir a direccion fisica
    direccionFisica = traducirDireccion(direccionLogica);
    
    // verificar que este dentro de los limites del programa
    // RB es menor o igual a direccion_fisica y, direccion_fisica es menor o igual a RL
    if (direccionFisica >= registrosCpu.rb && direccionFisica <= registrosCpu.rl) {
        return 1;
    }
    
    return 0;
}

// actualiza el codigo de condicion segun el resultado de una operacion
void actualizarCodigoCondicion(int resultado) {
    if (resultado > 9999999 || resultado < -9999999) {
        registrosCpu.psw.codigoCondicion = CC_DESBORDAMIENTO; // si pas el limite
    } else if (resultado == 0) {
        registrosCpu.psw.codigoCondicion = CC_CERO; // si es cero
    } else if (resultado < 0) {
        registrosCpu.psw.codigoCondicion = CC_NEGATIVO; // si es negativo
    } else {
        registrosCpu.psw.codigoCondicion = CC_POSITIVO; // si es positivo
    }
}

Palabra codificarPsw() {
    // se hace la multiplicacion para ponerlo en la posicion mas a la izquierda
    int valor = registrosCpu.psw.codigoCondicion * 10000000 + // 8va posicion
                registrosCpu.psw.modoOperacion * 1000000 + // 7ma posicion
                registrosCpu.psw.habilitarInterrupciones * 100000 + // 6ta posicion
                registrosCpu.psw.pc; // 5 ultimos digitos
    return enteroAPalabra(valor);
}

void decodificarPsw(Palabra pswPalabra) {
    int valor = palabraAEntero(pswPalabra);
    // asegurar que el valor sea positivo
    if (valor < 0) {
        valor = -valor;
    }
    // extraer campos 
    registrosCpu.psw.pc = valor % 100000;  // ultimos 5 digitos
    registrosCpu.psw.habilitarInterrupciones = (valor / 100000) % 10;  // 6to digito
    registrosCpu.psw.modoOperacion = (valor / 1000000) % 10;  // 7mo digito
    registrosCpu.psw.codigoCondicion = (valor / 10000000) % 10;  // 8vo digito
}

// obtiene el operando segun el modo de direccionamiento.
Palabra obtenerOperando(int modo, int valor) {
    Palabra operando;
    int direccionFisica;
    
    switch (modo) { // switch para obtener el operando segun el modo de direccionamiento
        case DIR_DIRECTO:
            // el valor es una direccion logica
            direccionFisica = traducirDireccion(valor);
            if (!verificarProteccionMemoria(direccionFisica)) { // si la direccion pertenece al programa
                interrupcionPendiente = 1; 
                codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA; // si no levanta una interrupcion
                operando = enteroAPalabra(0);
                return operando;
            }
            operando = leerMemoria(direccionFisica); // si todo esta bien ve a memoria y trae el dato
            break;
            
        case DIR_INMEDIATO:
            // el valor es el dato directamente
            operando = enteroAPalabra(valor);
            break;
            
        case DIR_INDEXADO:
            // el valor es un indice desde AC
            direccionFisica = traducirDireccion(valor + palabraAEntero(registrosCpu.ac));
            // la suma pasa a ser una direccion 
            if (!verificarProteccionMemoria(direccionFisica)) {
                interrupcionPendiente = 1;
                codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA; // si no levanta una interrupcion
                operando = enteroAPalabra(0);
                return operando;
            }
            operando = leerMemoria(direccionFisica); // si todo esta bien ve a memoria y trae el dato
            break;
            
        default:
            imprimirLog("ERROR: Modo de direccionamiento invalido");
            operando = enteroAPalabra(0);
    }
    return operando;
}

void inicializarCpu() {
    imprimirLog("Inicializando CPU...");
    
    // inicializar acumulador a 0
    registrosCpu.ac.signo = 0;
    registrosCpu.ac.digitos = 0;
    
    // inicializar MAR y MDR a 0
    registrosCpu.mar.signo = 0;
    registrosCpu.mar.digitos = 0;
    registrosCpu.mdr.signo = 0;
    registrosCpu.mdr.digitos = 0;
    
    // inicializar IR
    registrosCpu.ir.codigoOperacion = 0;
    registrosCpu.ir.direccionamiento = 0;
    registrosCpu.ir.valor = 0;
    
    // inicializar registros de proteccion de memoria
    // NOTA: Estos se establecen cuando se carga un programa
    registrosCpu.rb = INICIO_MEMORIA_USUARIO;  // 300
    registrosCpu.rl = TAMANO_MEMORIA - 1;      // 1999
    
    // inicializar pila al final de la memoria
    registrosCpu.rx = TAMANO_MEMORIA - 1;  // base de pila
    registrosCpu.sp = TAMANO_MEMORIA - 1;  // tope de pila
    
    // inicializar PSW
    // el cpu inicia en modo KERNEL para la carga del sistema operativo
    // luego cambiara a modo USUARIO cuando termine la inicializacion
    registrosCpu.psw.codigoCondicion = CC_CERO;
    registrosCpu.psw.modoOperacion = MODO_KERNEL;
    registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
    registrosCpu.psw.pc = 0;  // PC inicia en 0 (logico)
    
    // reiniciar contadores
    contadorCiclos = 0;
    intervaloReloj = 0;
    interrupcionPendiente = 0;
    codigoInterrupcionPendiente = -1;
    cpuEjecutando = 0;
    
    // reiniciar deteccion de bucles
    lastPC = -1;
    repetitionCount = 0;
    
    imprimirLog("CPU inicializado correctamente");
}

// bucle principal del CPU, ejecuta ciclos hasta fin de programa (segun RB/RL) o error fatal.
void ejecutarCpu() {
    cpuEjecutando = 1;
    imprimirLog("Iniciando ejecucion del CPU");
    
    // bucle principal de ejecucion
    while (cpuEjecutando) {
        // ejecutar un ciclo de instruccion
        if (!cicloCpu()) {
            // cicloCpu retorno 0, detener ejecucion
            break;
        }
    }
    
    // mostrar estado final
    imprimirLog("Devolviendo control a la consola");
}

int cicloCpu() {
    char buffer[100];
    
    // deteccion de bucle infinito 
    // si el PC no cambia durante 5 ciclos
    if (registrosCpu.psw.pc == lastPC) {
        repetitionCount++;
        if (repetitionCount >= 5) {
            imprimirLog("ALERTA: Posible bucle infinito detectado (PC estatico). Deteniendo ejecucion.");
            cpuEjecutando = 0;
            return 0;
        }
    } else {
        lastPC = registrosCpu.psw.pc; // actualiza el pc
        repetitionCount = 0; // reinicia el contador
    }
    
    // antes de la busqueda verificar fin por RL o PC fuera de RB
    int direccionFisicaPC = traducirDireccion(registrosCpu.psw.pc);
    // si PC > RL -> fin del programa 
    if (direccionFisicaPC > registrosCpu.rl) {
        imprimirLog("Detectada ultima instruccion, fin de programa");
        cpuEjecutando = 0;
        return 0;
    }
    // si PC < RB -> direccion invalida
    if (direccionFisicaPC < registrosCpu.rb) {
        imprimirLog("ERROR: PC < RB - Direccion invalida");
        interrupcionPendiente = 1;
        codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
        return 1;
    }

    // busqueda
    faseFetch();
    
    // decodificacion
    faseDecode();
    
    sprintf(buffer, "FETCH-DECODE: Op=%02d Dir=%d Val=%05d en PC=%05d",
            registrosCpu.ir.codigoOperacion,
            registrosCpu.ir.direccionamiento,
            registrosCpu.ir.valor,
            registrosCpu.psw.pc - 1);
    imprimirLog(buffer);
    
    // ejecucion
    faseExecute();
    
    // verificacion de interrupciones
    // interrupciones generadas por la instruccion (pendientes/fatales)
    if (interrupcionPendiente && registrosCpu.psw.habilitarInterrupciones) {
        // intentar manejar la interrupcion
        if (!manejarInterrupcion(codigoInterrupcionPendiente)) {
            // interrupcion FATAL -> detener CPU
            cpuEjecutando = 0;
        }
        // limpiar banderas
        interrupcionPendiente = 0;
        codigoInterrupcionPendiente = -1;
    }
    
    // interrupcion de reloj
    if (intervaloReloj > 0) {
        contadorCiclos++;
        if (contadorCiclos >= intervaloReloj) {
            contadorCiclos = 0;
            if (registrosCpu.psw.habilitarInterrupciones) {
                manejarInterrupcion(INT_TIMER);
            }
        }
    }
    
    // interrupcion de E/S (DMA)
    if (verificarInterrupcionDma() && registrosCpu.psw.habilitarInterrupciones) {
        interrupcionPendienteDma = 0;
        manejarInterrupcion(INT_IO_DONE);
    }

    // si hubo error fatal, cpuEjecutando sera 0
    return cpuEjecutando;
}

void faseFetch() {
    int direccionFisica;
    
    // traducir PC (logico) a direccion fisica
    direccionFisica = traducirDireccion(registrosCpu.psw.pc);
    
    // MAR <- direccion fisica (la direccion real de memoria)
    registrosCpu.mar = enteroAPalabra(direccionFisica);
    
    // verificar proteccion de memoria
    if (!verificarProteccionMemoria(direccionFisica)) {
        interrupcionPendiente = 1;
        codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
        return;
    }
    
    // MDR <- memoria[direccion_fisica]
    registrosCpu.mdr = leerMemoria(direccionFisica);
    
    // PC <- PC + 1
    registrosCpu.psw.pc++;
}

void faseDecode() {
    // IR <- MDR (la instruccion esta en MDR)
    // Formato: 
    // [Signo][OODDVVVVV] 
    // OO=opcode (2d), D=modo (1d), VVVVV=valor (5d)
    int instruccionCompleta = registrosCpu.mdr.digitos;

    // extraer campos de la instruccion 
    registrosCpu.ir.codigoOperacion = instruccionCompleta / 1000000;        // Primeros 2
    registrosCpu.ir.direccionamiento = (instruccionCompleta / 100000) % 10; // 3er digito
    registrosCpu.ir.valor = instruccionCompleta % 100000;                   // Ultimos 5
}

int faseExecute() {
    // obtener los campos de la instruccion
    int opcode = registrosCpu.ir.codigoOperacion;
    int modo = registrosCpu.ir.direccionamiento;
    int valor = registrosCpu.ir.valor;
    // declarar variables para hacer las operaciones
    Palabra operando;
    int resultado, valorAc, valorOp;
    int direccionFisica;
    char buffer[100];
    
    // verificar si el cpu esta en modo usuario y la instruccion es privilegiada
    if (registrosCpu.psw.modoOperacion == MODO_USUARIO && esInstruccionPrivilegiada(opcode)) {
        sprintf(buffer, "ERROR: Instruccion privilegiada %02d en modo usuario", opcode);
        imprimirLog(buffer);
        interrupcionPendiente = 1; // levantar bandera de interrupcion
        codigoInterrupcionPendiente = INT_INSTRUCCION_INVALIDA;
        return 1;
    }
    
    // ejecutar segun opcode
    switch (opcode) {
        // GRUPO 1: ARITMETICAS (SUM, RES, MULT)
        case OP_SUM:
        case OP_RES:
        case OP_MULT:
            operando = obtenerOperando(modo, valor); // trae el dato 
            valorAc = palabraAEntero(registrosCpu.ac); // valor del ac a num
            valorOp = palabraAEntero(operando); // valor del operano a num
            // Realizar operacion segun opcode
            if (opcode == OP_SUM) resultado = valorAc + valorOp; // suma
            else if (opcode == OP_RES) resultado = valorAc - valorOp; // resta
            else resultado = valorAc * valorOp; // multiplicacion
            registrosCpu.ac = enteroAPalabra(resultado); // resultado a ac
            actualizarCodigoCondicion(resultado); // actualizar banderas
            break;
            
        case OP_DIVI:  // 03: division
            operando = obtenerOperando(modo, valor); // trae el dato
            valorOp = palabraAEntero(operando); // dato a num
            if (valorOp == 0) { // si es cero error
                imprimirLog("ERROR: Division por cero");
                interrupcionPendiente = 1; // levanta bandera
                codigoInterrupcionPendiente = INT_OVERFLOW;
                return 1;
            }
            valorAc = palabraAEntero(registrosCpu.ac); // ac a num
            resultado = valorAc / valorOp; // operacion
            registrosCpu.ac = enteroAPalabra(resultado); // resultado a ac
            actualizarCodigoCondicion(resultado); // actualizar banderas
            break;
            
        // GRUPO 2: TRANSFERENCIA AC-MEMORIA
        case OP_LOAD:  // 04: cargar
            operando = obtenerOperando(modo, valor); // trae el dato de la ram
            registrosCpu.ac = operando; // lo guarda en el ac
            break;
            
        case OP_STR:  // 05: guardar
            if (modo == DIR_INMEDIATO) { // en este modo no funciona
                imprimirLog("ERROR: STR no soporta modo inmediato");
                return 1;
            }
            if (modo == DIR_INDEXADO) { // se suma para obtener la direccion
                direccionFisica = traducirDireccion(valor + palabraAEntero(registrosCpu.ac));
            } else {
                direccionFisica = traducirDireccion(valor); // direccion logica a fisica
            }
            if (!verificarProteccionMemoria(direccionFisica)) { // si no es del programa salta
                interrupcionPendiente = 1;
                codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                return 1;
            }
            escribirMemoria(direccionFisica, registrosCpu.ac); // guarda el ac en la ram
            break;
            
        // GRUPO 3: TRANSFERENCIA AC-REGISTROS (PRIVILEGIADAS)
        case OP_LOADRX:  // 06: cargar rx
            registrosCpu.ac = enteroAPalabra(registrosCpu.rx); // ac guarda lo que hay en rx
            break;
            
        case OP_STRRX:  // 07: guardar rx
            registrosCpu.rx = palabraAEntero(registrosCpu.ac); // rx guarda lo que hay en ac
            break;
            
        // GRUPO 4: COMPARACION Y SALTOS
        case OP_COMP:  // 08: comparar
            operando = obtenerOperando(modo, valor); // trae el dato
            valorAc = palabraAEntero(registrosCpu.ac); // ac a num
            valorOp = palabraAEntero(operando); // dato a num
            if (valorAc == valorOp) {
                registrosCpu.psw.codigoCondicion = CC_CERO; // si son iguales
            } else if (valorAc < valorOp) {
                registrosCpu.psw.codigoCondicion = CC_NEGATIVO; // si ac es menor
            } else {
                registrosCpu.psw.codigoCondicion = CC_POSITIVO; // si ac es mayor
            }
            break;
            
        // GRUPO 4: SALTOS CONDICIONALES (comparan AC con M[SP])
        case OP_JMPE:
        case OP_JMPNE:
        case OP_JMPLT:
        case OP_JMPLGT:
            valorAc = palabraAEntero(registrosCpu.ac); // ac a num
            valorOp = palabraAEntero(leerMemoria(registrosCpu.sp)); // dato en la pila a num
            // evaluar condicion segun opcode
            int saltar = 0;
            if (opcode == OP_JMPE && valorAc == valorOp) saltar = 1; // si son iguales
            else if (opcode == OP_JMPNE && valorAc != valorOp) saltar = 1; // si son distintos
            else if (opcode == OP_JMPLT && valorAc < valorOp) saltar = 1; // si es menor
            else if (opcode == OP_JMPLGT && valorAc > valorOp) saltar = 1; // si es mayor
            // ejecutar salto si corresponde
            if (saltar) {
                if (!verificarDireccionSalto(valor)) { // si la direccion no pertenece al programa salta
                    imprimirLog("ERROR: Salto fuera de limites RB/RL");
                    interrupcionPendiente = 1; // levanta bandera
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.psw.pc = valor; // actualiza el pc con el salto
            }
            break;
            
        // GRUPO 5: CONTROL DEL SISTEMA
        case OP_SVC:  // 13: llamada al sistema
            imprimirLog("SVC: Llamada al sistema");
            interrupcionPendiente = 1; // levanta bandera
            codigoInterrupcionPendiente = INT_SVC; // le avisa al so
            break;
            
        case OP_RETRN:  // 14: retorno
            // pop pc de la pila 
            if (registrosCpu.sp >= registrosCpu.rx) { // si ya no hay nada en la pila sale
                imprimirLog("ERROR: Stack underflow al hacer RETRN");
                interrupcionPendiente = 1;
                codigoInterrupcionPendiente = INT_UNDERFLOW;
                return 1;
            }
            registrosCpu.psw.pc = palabraAEntero(leerMemoria(registrosCpu.sp)); // recupera el pc de la pila
            registrosCpu.sp++; // actualiza el sp
            break;
            
        case OP_HAB:  // 15: habilitar interrupciones
            registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS; // se activan las interrupciones
            imprimirLog("Interrupciones habilitadas");
            break;
            
        case OP_DHAB:  // 16: deshabilitar interrupciones
            registrosCpu.psw.habilitarInterrupciones = INT_DESHABILITADAS; // se desactivan
            imprimirLog("Interrupciones deshabilitadas");
            break;
            
        case OP_TTI:  // 17: intervalo de reloj (privilegiada)
            intervaloReloj = valor; // se guarda el valor en el reloj
            contadorCiclos = 0; // se reinicia el contador
            sprintf(buffer, "Timer configurado: interrupcion cada %d ciclos", valor);
            imprimirLog(buffer);
            break;
            
        case OP_CHMOD:  // 18: Cambiar modo (privilegiada)
            if (registrosCpu.psw.modoOperacion == MODO_KERNEL) {
                registrosCpu.psw.modoOperacion = MODO_USUARIO;
                imprimirLog("Cambio a modo USUARIO");
            } else {
                // Solo se puede cambiar a kernel desde kernel
                imprimirLog("ERROR: No se puede cambiar a kernel desde usuario");
            }
            break;
            
        // GRUPO 6: REGISTROS BASE/LIMITE/PILA (privilegiadas)
        case OP_LOADRB:  // 19: cargar rb
        case OP_LOADRL:  // 21: cargar rl
        case OP_LOADSP:  // 23: cargar sp
            if (opcode == OP_LOADRB) registrosCpu.ac = enteroAPalabra(registrosCpu.rb); // ac guarda el rb
            else if (opcode == OP_LOADRL) registrosCpu.ac = enteroAPalabra(registrosCpu.rl); // ac guarda el rl
            else registrosCpu.ac = enteroAPalabra(registrosCpu.sp); // ac guarda el sp
            break;
            
        case OP_STRRB:  // 20: guardar rb
            {
                int nuevoRB = palabraAEntero(registrosCpu.ac); // nuevo valor de ac a num
                if (nuevoRB < INICIO_MEMORIA_USUARIO || nuevoRB >= registrosCpu.rl) { // validaciones del rb
                    imprimirLog("ERROR: Valor invalido para RB");
                    interrupcionPendiente = 1; // levanta bandera
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.rb = nuevoRB; // actualiza el rb
            }
            break;
            
        case OP_STRRL:  // 22: guardar rl
            {
                int nuevoRL = palabraAEntero(registrosCpu.ac); // nuevo valor de ac a num
                if (nuevoRL < registrosCpu.rb || nuevoRL >= TAMANO_MEMORIA) { // validacion
                    imprimirLog("ERROR: Valor invalido para RL");
                    interrupcionPendiente = 1;
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.rl = nuevoRL; // actualiza rl
            }
            break;
            
        case OP_STRSP:  // 24: guardar sp
            {
                int nuevoSP = palabraAEntero(registrosCpu.ac); // nuevo valor a num
                if (nuevoSP < registrosCpu.rb || nuevoSP > registrosCpu.rx) { // validacion de la pila
                    imprimirLog("ERROR: Valor invalido para SP");
                    interrupcionPendiente = 1;
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.sp = nuevoSP; // actualiza sp
            }
            break;
            
        // GRUPO 7: PILA
        case OP_PSH:  // 25: push
            registrosCpu.sp--; // primero baja el puntero de la pila
            if (registrosCpu.sp < registrosCpu.rb) { // validacion del programa
                imprimirLog("ERROR: Stack overflow");
                interrupcionPendiente = 1; // levanta bandera
                codigoInterrupcionPendiente = INT_OVERFLOW;
                return 1;
            }
            escribirMemoria(registrosCpu.sp, registrosCpu.ac); // guarda el ac en la pila
            break;
            
        case OP_POP:  // 26: pop
            // underflow: SP == RX => pila vacia
            if (registrosCpu.sp >= registrosCpu.rx) { // si no hay nada en la pila sale
                imprimirLog("ERROR: Stack underflow");
                interrupcionPendiente = 1;
                codigoInterrupcionPendiente = INT_UNDERFLOW;
                return 1;
            }
            registrosCpu.ac = leerMemoria(registrosCpu.sp); // recupera de la pila al ac
            registrosCpu.sp++; // actualiza el puntero subiendo
            break; 
            
        // GRUPO 8: SALTO INCONDICIONAL
        case OP_J:  // 27: salto de fe
            // verificar que la direccion destino este dentro de limites
            if (!verificarDireccionSalto(valor)) { // si no pertenece al programa salta
                imprimirLog("ERROR: Salto fuera de limites RB/RL");
                interrupcionPendiente = 1; // levanta bandera
                codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                return 1;
            }
            registrosCpu.psw.pc = valor; // actualiza el pc con el salto (salto incondicional)
            break;
            
        // GRUPO 9: DMA (Instrucciones de E/S)
        case OP_SDMAP:  // 28: pista del DMA
            dma.pistaSeleccionada = valor; // guarda la pista
            sprintf(buffer, "DMA: Pista establecida a %d", valor);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAC:  // 29: cilindro del DMA
            dma.cilindroSeleccionado = valor; // guarda el cilindro
            sprintf(buffer, "DMA: Cilindro establecido a %d", valor);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAS:  // 30: sector del DMA
            dma.sectorSeleccionado = valor; // guarda el sector
            sprintf(buffer, "DMA: Sector establecido a %d", valor);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAIO:  // 31: direccion de E/S
            dma.direccionIo = valor; // guarda si lee o escribe
            sprintf(buffer, "DMA: Direccion E/S establecida a %d (%s)",
                    valor, valor == 0 ? "LEER" : "ESCRIBIR");
            imprimirLog(buffer);
            break;
            
        case OP_SDMAM:  // 32: direccion de memoria
            dma.direccionMemoria = valor; // guarda la direccion de la memoria ram
            sprintf(buffer, "DMA: Direccion de memoria establecida a %d", valor);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAON:  // 33: iniciar dma
            imprimirLog("DMA: Iniciando transferencia...");
            iniciarTransferenciaDma(); // dispara el hilo para mover los datos
            break;
            
        default:
            sprintf(buffer, "ERROR: Opcode invalido %02d", opcode);
            imprimirLog(buffer);
            interrupcionPendiente = 1;
            codigoInterrupcionPendiente = INT_INSTRUCCION_INVALIDA;
            return 1;
    }
    
    return 1;  // continuar ejecucion
}

void guardarContexto() {
    int spOriginal = registrosCpu.sp;
    
    imprimirLog("Guardando contexto en la pila...");
    
    // Verificar espacio para 6 palabras
    if (registrosCpu.sp - 6 < 0) {
        printf("ERROR FATAL: Overflow de pila del SO\n");
        imprimirLog("ERROR FATAL: Overflow de pila del SO");
        cpuEjecutando = 0;
        return;
    }
    
    // apilar registros: AC, RB, RL, RX, SP original, PSW 
    registrosCpu.sp--; escribirMemoria(registrosCpu.sp, registrosCpu.ac);
    registrosCpu.sp--; escribirMemoria(registrosCpu.sp, enteroAPalabra(registrosCpu.rb));
    registrosCpu.sp--; escribirMemoria(registrosCpu.sp, enteroAPalabra(registrosCpu.rl));
    registrosCpu.sp--; escribirMemoria(registrosCpu.sp, enteroAPalabra(registrosCpu.rx));
    registrosCpu.sp--; escribirMemoria(registrosCpu.sp, enteroAPalabra(spOriginal));
    registrosCpu.sp--; escribirMemoria(registrosCpu.sp, codificarPsw());
    
    imprimirLog("Contexto guardado exitosamente");
}

void restaurarContexto() {
    imprimirLog("Restaurando contexto desde la pila...");
    
    // desapilar en orden inverso: PSW, SP original, RX, RL, RB, AC
    decodificarPsw(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
    int spOriginal = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
    registrosCpu.rx = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
    registrosCpu.rl = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
    registrosCpu.rb = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
    registrosCpu.ac = leerMemoria(registrosCpu.sp); registrosCpu.sp++;
    
    // restaurar SP al valor original
    registrosCpu.sp = spOriginal;
    imprimirLog("Contexto restaurado exitosamente");
}

// maneja una interrupcion retorna 1 si es recuperable, 0 si es fatal
int manejarInterrupcion(int codigoInterrupcion) {

    int esRecuperable = 0;
    // guardar contexto
    guardarContexto();
    
    // cambiar a modo kernel
    registrosCpu.psw.modoOperacion = MODO_KERNEL;
    
    // deshabilitar interrupciones
    registrosCpu.psw.habilitarInterrupciones = INT_DESHABILITADAS;
    
    // determinar si es recuperable y ejecutar manejador
    const char *desc = "Desconocida";
    
    // es importante aclarar que las interrupciones se identifican, pero se manejan solo clasificandolas en recuperables o no
    switch (codigoInterrupcion) {
        case INT_SYSCALL_INVALIDA:  // 0
            desc = "Syscall invalida";
            esRecuperable = 0;
            break;
            
        case INT_CODIGO_INVALIDO:  // 1
            desc = "Codigo de interrupcion invalido";
            esRecuperable = 0;
            break;
            
        case INT_SVC:  // 2
            desc = "Llamada al sistema (SVC)";
            esRecuperable = 1;
            break;
            
        case INT_TIMER:  // 3
            desc = "Temporizador (Clock)";
            esRecuperable = 1;
            break;
            
        case INT_IO_DONE:  // 4
            desc = "Operacion E/S completada";
            esRecuperable = 1;
            break;
            
        case INT_INSTRUCCION_INVALIDA:  // 5
            desc = "Instruccion invalida o privilegiada";
            esRecuperable = 0;
            break;
            
        case INT_DIRECCION_INVALIDA: // 6
            desc = "Direccionamiento invalido (Violacion de Memoria)";
            esRecuperable = 0;
            break;

        case INT_UNDERFLOW: // 7
            desc = "Stack Underflow";
            esRecuperable = 0;
            break;

        case INT_OVERFLOW: // 8
            desc = "Overflow Aritmetico";
            esRecuperable = 0;
            break;
    }

    // mensaje unificado y claro tanto para consola como log
    // logInterrupcion se imprime en ambos destinos
    char tipoInt[20];
    if (esRecuperable) strcpy(tipoInt, "RECUPERABLE");
    else strcpy(tipoInt, "FATAL");

    logInterrupcion("Ciclo %d | Interrupcion %d (%s): %s", contadorCiclos, codigoInterrupcion, tipoInt, desc);

    if (!esRecuperable) {
        logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    } else {
        logCpu("Manejando interrupcion recuperable: %s", desc);
    }
    
    // si es recuperable, restaurar contexto y volver a modo usuario
    if (esRecuperable) {
        restaurarContexto();
        imprimirLog("Retornando de interrupcion");
    } else {
        imprimirLog("Deteniendo ejecucion - Terminando programa");
        cpuEjecutando = 0;
    }
    
    registrosCpu.psw.modoOperacion = MODO_USUARIO; // cambiar a modo usuario   
    registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS; // habilitar interrupciones
    return esRecuperable;
}

