// aqui definimos todo lo que tiene que ver con el cerebro de la maquina (el CPU)
// estan las estructuras para guardar el estado y todas las funciones que necesitamos
// para que el procesador entienda y ejecute las instrucciones
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/cpu.h"
#include "../include/hardware.h"
#include "../include/memoria.h"
#include "../include/disco.h"
#include "../include/dma.h"
#include "../include/logger.h"
#include "../include/procesos.h"

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
int interrupcionesPendientes[NUM_INTERRUPCIONES] = {0};
RutinaManejadora vectorInterrupciones[NUM_INTERRUPCIONES];

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
        interrupcionesPendientes[INT_OVERFLOW] = 1;
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
    
    // Regla de Fase 2: Un proceso accede a su pila EXCLUSIVAMENTE mediante PSH/POP. 
    // Significa que las instrucciones ordinarias no pueden leer/escribir donde esta la pila (desde SP hacia arriba hasta RL).
    // NOTA: Si la pila esta vacia, SP == RX + 1 (o SP == RL + 1 si RX = RL). El limite permitido llega hasta SP - 1.
    
    // Proteccion 1: No bajar del Registro Base (Aislar de otros procesos o Kernel abajo)
    if (direccionFisica < registrosCpu.rb) {
        return 0; // Violacion de Memoria (Limites Inferiores)
    }
    
    // Proteccion 2: No tocar ni exceder la Pila del Proceso (Aislar de su propia Pila y otros procesos arriba)
    if (direccionFisica >= registrosCpu.sp) {
        return 0; // Violacion de Memoria (Limites de Pila/Superiores)
    }
    
    return 1;
}

int esInstruccionPrivilegiada(int opcode) {
    // instrucciones de usuario
    if (opcode >= 0 && opcode <= 5) return 0;   // aritmeticas y transferencia
    if (opcode >= 8 && opcode <= 13) return 0;  // comparacion, saltos, SVC
    if (opcode == 17) return 0;                 // temporizador (TTI) liberado para caso6
    if (opcode >= 25 && opcode <= 33) return 0; // pila, salto incondicional, DMA
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
                interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1; // si no levanta una interrupcion
                operando = enteroAPalabra(0);
                return operando;
            }
            operando = leerMemoria(direccionFisica); // si todo esta bien ve a memoria y trae el dato
            break;
            
        case DIR_INMEDIATO:
            // el valor es el dato directamente
            operando = enteroAPalabra(valor);
            // Si la instrucción en MDR tenía un signo negativo (ej, carga inmediata con signo)
            if (registrosCpu.mdr.signo == 1) {
                operando.signo = 1;
            }
            break;
            
        case DIR_INDEXADO:
            // el valor es un indice desde AC
            direccionFisica = traducirDireccion(valor + palabraAEntero(registrosCpu.ac));
            // la suma pasa a ser una direccion 
            if (!verificarProteccionMemoria(direccionFisica)) {
                interrupcionPendiente = 1;
                interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1; // si no levanta una interrupcion
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
    for(int i=0; i<NUM_INTERRUPCIONES; i++) interrupcionesPendientes[i] = 0;
    cpuEjecutando = 0;
    
    // reiniciar deteccion de bucles
    lastPC = -1;
    repetitionCount = 0;
    
    // inicializar Vector
    inicializarVectorInterrupciones();
    imprimirLog("CPU inicializado correctamente");
}

// bucle principal del CPU, ejecuta ciclos hasta fin de programa (segun RB/RL) o error fatal.
void ejecutarCpu() {
    cpuEjecutando = 1;

    // Regla de Fase 1: El quantum de tiempo esta fijado en dos (2) tics de reloj
    intervaloReloj = 2; // Dos instrucciones completas
    contadorCiclos = 0;

    imprimirLog("Iniciando ejecucion planificada del CPU (RR con q=2)");

    // El primer proceso en activarse ya fue configurado por main.c
    if (procesoEnEjecucion == -1) {
        imprimirLog("No hay procesos cargados o en ejecucion. Saliendo de simulacion...");
        cpuEjecutando = 0;
        return;
    }
    
    int siguiente;
    
    // bucle principal de ejecucion
    while (cpuEjecutando) {
        // ejecutar un ciclo de instruccion
        if (!cicloCpu()) {
            
            // Si el CPU se detuvo pero en realidad el proceso acabo o murio, podemos ver si hay otro
            if (!cpuEjecutando) {
                 // Destruir el actual
                 if (procesoEnEjecucion != -1) {
                     destruirProceso(procesoEnEjecucion);
                 }
                 
                 // Buscar otro
                 siguiente = planificarSiguienteProceso();
                 if (siguiente != -1) {
                     // Todavia hay trabajo
                     cpuEjecutando = 1;
                     despacharProceso(siguiente);
                 } else {
                     if (hayProcesosVivos()) {
                         // CPU inactiva (SO Ocioso). Pero hay procesos durmientes o DMA
                         imprimirLog("SO entrando en modo IDLE (Tics muertos)...");
                         while (siguiente == -1 && hayProcesosVivos()) {
                             contadorCiclos++;
                             actualizarProcesosDormidos();
                             if (contadorCiclos >= intervaloReloj) {
                                  contadorCiclos = 0;
                             }
                             siguiente = planificarSiguienteProceso();
                         }
                         
                         // Si salio del bucle porque desperto alguien, a correr
                         if (siguiente != -1) {
                             cpuEjecutando = 1;
                             despacharProceso(siguiente);
                         } else {
                             imprimirLog("No hay mas procesos. Todas las tareas finalizadas.");
                             break;
                         }
                         
                     } else {
                         imprimirLog("No hay mas procesos. Todas las tareas finalizadas.");
                         break; // Se acabo de verdad
                     }
                 }
            }
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
        sprintf(buffer, "DEBUG: Fin por RL superado. ProcID: %d, PC Logico: %d, DirFisica: %d, RL: %d, SP: %d", procesoEnEjecucion, registrosCpu.psw.pc, direccionFisicaPC, registrosCpu.rl, registrosCpu.sp);
        imprimirLog(buffer);
        imprimirLog("Detectada ultima instruccion, fin de programa");
        guardarContexto(); // Simular interrupcion para alinear el pop del despachador
        cpuEjecutando = 0;
        return 0;
    }
    
    // Verificacion de instruccion nula ANTES de Fetch-Decode
    Palabra probeStr = leerMemoria(direccionFisicaPC);
    if (palabraAEntero(probeStr) == 0 && probeStr.signo == 0) {
        sprintf(buffer, "DEBUG: Fin por instruccion NULA. ProcID: %d, PC Logico: %d, DirFisica: %d", procesoEnEjecucion, registrosCpu.psw.pc, direccionFisicaPC);
        imprimirLog(buffer);
        imprimirLog("Detectada ultima instruccion, fin de programa");
        guardarContexto(); // Simular interrupcion para alinear el pop del despachador
        cpuEjecutando = 0;
        return 0;
    }
    // si PC < RB -> direccion invalida
    if (direccionFisicaPC < registrosCpu.rb) {
        imprimirLog("ERROR: PC < RB - Direccion invalida");
        interrupcionPendiente = 1;
        interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
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
    
    // interrupcion de reloj
    if (intervaloReloj > 0) {
        contadorCiclos++;
        actualizarProcesosDormidos(); // Actualiza a procesos suspendidos 1 tic por ciclo
        if (contadorCiclos >= intervaloReloj) {
            contadorCiclos = 0;
            interrupcionesPendientes[INT_TIMER] = 1;
            interrupcionPendiente = 1;
        }
    }
    
    // interrupcion de E/S (DMA)
    if (verificarInterrupcionDma()) {
        interrupcionPendienteDma = 0;
        interrupcionesPendientes[INT_IO_DONE] = 1;
        interrupcionPendiente = 1;
    }

    // procesado agrupado, estricto y jerarquico (La CPU atiende todo de golpe)
    if (interrupcionPendiente && registrosCpu.psw.habilitarInterrupciones) {
        if (!procesarInterrupcionesPendientes()) {
            cpuEjecutando = 0;
        }
        interrupcionPendiente = 0;
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
        interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
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
        interrupcionesPendientes[INT_INSTRUCCION_INVALIDA] = 1;
        return 1;
    }
    // ejecutar segun opcode
    switch (opcode) {
        case OP_SUM:
        case OP_RES:
        case OP_MULT:
            operando = obtenerOperando(modo, valor); // trae el dato 
            valorAc = palabraAEntero(registrosCpu.ac); // valor del ac a num
            valorOp = palabraAEntero(operando); // valor del operano a num
            // Realizar operacion segun opcode
            if (opcode == OP_SUM) {
                resultado = valorAc + valorOp; // suma
                sprintf(buffer, "SUM: %d + %d = %d", valorAc, valorOp, resultado);
                imprimirLog(buffer);
            }
            else if (opcode == OP_RES) {
                resultado = valorAc - valorOp; // resta
                sprintf(buffer, "RES: %d - %d = %d", valorAc, valorOp, resultado);
                imprimirLog(buffer);
            }
            else {
                resultado = valorAc * valorOp; // multiplicacion
                sprintf(buffer, "MULT: %d * %d = %d", valorAc, valorOp, resultado);
                imprimirLog(buffer);
            }
            registrosCpu.ac = enteroAPalabra(resultado); // resultado a ac
            actualizarCodigoCondicion(resultado); // actualizar banderas
            break;
            
        case OP_DIVI:  // 03: division
            operando = obtenerOperando(modo, valor); // trae el dato
            valorOp = palabraAEntero(operando); // dato a num
            if (valorOp == 0) { // si es cero error
                imprimirLog("ERROR: Division por cero");
                interrupcionPendiente = 1; // levanta bandera
                interrupcionesPendientes[INT_OVERFLOW] = 1;
                return 1;
            }
            valorAc = palabraAEntero(registrosCpu.ac); // ac a num
            resultado = valorAc / valorOp; // operacion
            registrosCpu.ac = enteroAPalabra(resultado); // resultado a ac
            actualizarCodigoCondicion(resultado); // actualizar banderas
            break;
            
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
                interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
                return 1;
            }
            escribirMemoria(direccionFisica, registrosCpu.ac); // guarda el ac en la ram
            break;
            
        case OP_LOADRX:  // 06: cargar rx
            registrosCpu.ac = enteroAPalabra(registrosCpu.rx); // ac guarda lo que hay en rx
            break;
            
        case OP_STRRX:  // 07: guardar rx
            registrosCpu.rx = palabraAEntero(registrosCpu.ac); // rx guarda lo que hay en ac
            break;
            
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
            
        case OP_JMPE:
        case OP_JMPNE:
        case OP_JMPLT:
        case OP_JMPLGT:
            valorAc = palabraAEntero(registrosCpu.ac); // ac a num
            Palabra palabraPila = leerMemoria(registrosCpu.sp); // dato en la pila
            valorOp = palabraAEntero(palabraPila); // a num (magnitud y signo real)
            
            // evaluar condicion segun opcode
            int saltar = 0;
            if (opcode == OP_JMPE && valorAc == valorOp) saltar = 1; // si son iguales
            else if (opcode == OP_JMPNE && valorAc != valorOp) saltar = 1; // si son distintos
            else if (opcode == OP_JMPLT && valorAc < valorOp) saltar = 1; // si es menor
            else if (opcode == OP_JMPLGT && valorAc > valorOp) saltar = 1; // si es mayor

            sprintf(buffer, "JMP/CMP: AC=%d vs M[SP]=%d. Opcode=%d. Salta=%d", valorAc, valorOp, opcode, saltar);
            imprimirLog(buffer);

            // ejecutar salto si corresponde
            if (saltar) {
                if (!verificarDireccionSalto(valor)) { // si la direccion no pertenece al programa salta
                    imprimirLog("ERROR: Salto fuera de limites RB/RL");
                    interrupcionPendiente = 1; // levanta bandera
                    interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
                    return 1;
                }
                registrosCpu.psw.pc = valor; // actualiza el pc con el salto
            }
            break;
            
        case OP_SVC:  // 13: llamada al sistema
            imprimirLog("SVC: Llamada al sistema");
            interrupcionPendiente = 1; // levanta bandera
            interrupcionesPendientes[INT_SVC] = 1; // le avisa al so
            break;
            
        case OP_RETRN:  // 14: retorno
            // pop pc de la pila 
            if (registrosCpu.sp >= registrosCpu.rx) { // si ya no hay nada en la pila sale
                imprimirLog("ERROR: Stack underflow al hacer RETRN");
                interrupcionPendiente = 1;
                interrupcionesPendientes[INT_UNDERFLOW] = 1;
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
                    interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
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
                    interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
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
                    interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
                    return 1;
                }
                registrosCpu.sp = nuevoSP; // actualiza sp
            }
            break;
            
        case OP_PSH:  // 25: push
            registrosCpu.sp--; // primero baja el puntero de la pila
            if (registrosCpu.sp < registrosCpu.rb) { // colision con la base del programa
                imprimirLog("ERROR: Stack overflow");
                interrupcionPendiente = 1; 
                interrupcionesPendientes[INT_OVERFLOW] = 1;
                return 1;
            }
            // PSH permite operando inmediato o variable (ej. 25100050 = PSH Inmediato 50)
            if (modo != 0 || valor != 0) {
                operando = obtenerOperando(modo, valor);
                escribirMemoria(registrosCpu.sp, operando);
                sprintf(buffer, "PSH: Apilando operando %d", palabraAEntero(operando));
                imprimirLog(buffer);
            } else {
                // Compatibilidad legacy: 25000000 apila lo que haya en AC
                escribirMemoria(registrosCpu.sp, registrosCpu.ac);
                sprintf(buffer, "PSH: Apilando AC (%d)", palabraAEntero(registrosCpu.ac));
                imprimirLog(buffer);
            }
            break;
            
        case OP_POP:  // 26: pop
            // underflow: si SP alcanzo o excedio el limite original RX de la pila
            if (registrosCpu.sp > registrosCpu.rx) { // si no hay nada en la pila sale
                imprimirLog("ERROR: Stack underflow");
                interrupcionPendiente = 1;
                interrupcionesPendientes[INT_UNDERFLOW] = 1;
                return 1;
            }
            registrosCpu.ac = leerMemoria(registrosCpu.sp); // recupera de la pila al ac
            registrosCpu.sp++; // actualiza el puntero de pila encogiendola
            break; 
            
        case OP_J:  // 27: salto de fe
            // verificar que la direccion destino este dentro de limites
            if (!verificarDireccionSalto(valor)) { // si no pertenece al programa salta
                imprimirLog("ERROR: Salto fuera de limites RB/RL");
                interrupcionPendiente = 1; // levanta bandera
                interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
                return 1;
            }
            registrosCpu.psw.pc = valor; // actualiza el pc con el salto (salto incondicional)
            break;
            
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
            // En nuestra fase 1 sin SO planificador multiproceso,
            // el CPU necesita esperar a que el DMA termine para no asfixiar las pruebas.
            while (dma.ocupado) {
                // Pequeña pausa para no saturar 100% de CPU
                usleep(1000); 
            }
            break;
            
        default:
            sprintf(buffer, "ERROR: Opcode invalido %02d", opcode);
            imprimirLog(buffer);
            interrupcionPendiente = 1;
            interrupcionesPendientes[INT_INSTRUCCION_INVALIDA] = 1;
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

// === INICIO DEL VECTOR DE INTERRUPCIONES ===
int rutinaManejadora_SyscallInvalida(void) {
    const char *desc = "Syscall invalida";
    logInterrupcion("Ciclo %d | Interrupcion %d (FATAL): %s", contadorCiclos, INT_SYSCALL_INVALIDA, desc);
    logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    return 0; // fatal
}

int rutinaManejadora_CodigoInvalido(void) {
    const char *desc = "Codigo de interrupcion invalido";
    logInterrupcion("Ciclo %d | Interrupcion %d (FATAL): %s", contadorCiclos, INT_CODIGO_INVALIDO, desc);
    logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    return 0; // fatal
}

int rutinaManejadora_SVC(void) {
    const char *desc = "Llamada al sistema (SVC)";
    logInterrupcion("Ciclo %d | Interrupcion %d (RECUPERABLE): %s", contadorCiclos, INT_SVC, desc);
    
    // AC se encuentra salvado en SP + 5
    int codigoSyscall = palabraAEntero(leerMemoria(registrosCpu.sp + 5));
    char buffer[128];
    
    // El SP real del usuario (antes del volcado de contexto de inteerupcion) esta guardado en SP + 1
    int spUsuarioVirtual = palabraAEntero(leerMemoria(registrosCpu.sp + 1));
    int parametro = 0;
    int estadoSalida = 1;

    switch (codigoSyscall) {
        case 1: // termina_prog (estado)
            if (spUsuarioVirtual > registrosCpu.rx) {
                logCpu("Error Syscall 1: Pila Vacia (Underflow)");
            } else {
                parametro = palabraAEntero(leerMemoria(spUsuarioVirtual));
                spUsuarioVirtual++;
                escribirMemoria(registrosCpu.sp + 1, enteroAPalabra(spUsuarioVirtual)); // Actualizar el volcado
            }
            sprintf(buffer, "SYS_EXIT: Proceso finalizado con estado %d", parametro);
            imprimirLog(buffer);
            printf("[SYSCALL] %s\n", buffer);
            cambiarEstadoProceso(procesoEnEjecucion, ESTADO_TERMINADO);
            
            // Forzar ceder el turno
            int procA = planificarSiguienteProceso();
            if (procA != -1) {
                despacharProceso(procA);
            } else {
                despacharProceso(-1);
                cpuEjecutando = 0;
            }
            estadoSalida = 2; // Contexto Cambiado
            break;
            
        case 2: // imprime_pantalla (valor)
            if (spUsuarioVirtual > registrosCpu.rx) {
                logCpu("Error Syscall 2: Pila Vacia (Underflow)");
            } else {
                parametro = palabraAEntero(leerMemoria(spUsuarioVirtual));
                spUsuarioVirtual++;
                escribirMemoria(registrosCpu.sp + 1, enteroAPalabra(spUsuarioVirtual));
            }
            sprintf(buffer, "SYS_PRINT: Proceso %d solicita imprimir el valor: %d", procesoEnEjecucion, parametro);
            imprimirLog(buffer);
            printf("[SYSCALL] %s\n", buffer);
            break;
            
        case 3: // leer_pantalla () -> Retorna en AC
            printf("\n[SYSCALL] Proceso %d solicita entrada. Ingrese un numero: ", procesoEnEjecucion);
            int entrada;
            if (scanf("%d", &entrada) != 1) {
                entrada = 0; 
                while(getchar() != '\n'); 
            }
            // Insertar retorno en lugar donde AC fue volcado (SP + 5)
            escribirMemoria(registrosCpu.sp + 5, enteroAPalabra(entrada));
            sprintf(buffer, "SYS_READ: Proceso inyecto el valor %d en AC", entrada);
            imprimirLog(buffer);
            break;
            
        case 4: // Dormir (tics)
            if (spUsuarioVirtual > registrosCpu.rx) {
                logCpu("Error Syscall 4: Pila Vacia (Underflow)");
            } else {
                parametro = palabraAEntero(leerMemoria(spUsuarioVirtual));
                spUsuarioVirtual++;
                escribirMemoria(registrosCpu.sp + 1, enteroAPalabra(spUsuarioVirtual));
            }
            sprintf(buffer, "SYS_SLEEP: Proceso %d solicito dormir %d tics", procesoEnEjecucion, parametro);
            imprimirLog(buffer);
            printf("[SYSCALL] %s\n", buffer);
            
            for (int i = 0; i < MAX_PROCESOS; i++) {
                if (tablaProcesos[i].id == procesoEnEjecucion) {
                    tablaProcesos[i].ticsDormido = parametro;
                    break;
                }
            }
            cambiarEstadoProceso(procesoEnEjecucion, ESTADO_DORMIDO);
            
            int procB = planificarSiguienteProceso();
            if (procB != -1) {
                despacharProceso(procB);
            } else {
                despacharProceso(-1);
                cpuEjecutando = 0;
            }
            estadoSalida = 2; // Contexto cambiado
            break;
            
        default:
            sprintf(buffer, "SYS_EXIT (Legacy): Codigo %d en AC no soportado. Forzando terminacion.", codigoSyscall);
            imprimirLog(buffer);
            printf("[SYSCALL] %s\n", buffer);
            cambiarEstadoProceso(procesoEnEjecucion, ESTADO_TERMINADO);
            
            int procC = planificarSiguienteProceso();
            if (procC != -1) {
                despacharProceso(procC);
            } else {
                despacharProceso(-1);
                cpuEjecutando = 0;
            }
            estadoSalida = 2; // Contexto cambiado
            break;
    }

    return estadoSalida;
}

int rutinaManejadora_Timer(void) {
    const char *desc = "Temporizador (Clock)";
    logInterrupcion("Ciclo %d | Interrupcion %d (RECUPERABLE): %s", contadorCiclos, INT_TIMER, desc);
    
    int siguiente_proceso = planificarSiguienteProceso();
    if (siguiente_proceso != -1) {
         despacharProceso(siguiente_proceso);
    } else {
         imprimirLog("Timer: No hay mas procesos listos. Se cede.");
         despacharProceso(-1);
         cpuEjecutando = 0; 
    }
    imprimirLog("Retornando de interrupcion de Quantum (Contexto provisto por Despachador BCP)");
    
    return 2; // recuperable sin restaurado automatico
}

int rutinaManejadora_IODone(void) {
    const char *desc = "Operacion E/S completada";
    logInterrupcion("Ciclo %d | Interrupcion %d (RECUPERABLE): %s", contadorCiclos, INT_IO_DONE, desc);
    logCpu("Manejando interrupcion recuperable: %s", desc);
    imprimirLog("Retornando de interrupcion");
    return 1; // recuperable
}

int rutinaManejadora_InstruccionInvalida(void) {
    const char *desc = "Instruccion invalida o privilegiada";
    logInterrupcion("Ciclo %d | Interrupcion %d (FATAL): %s", contadorCiclos, INT_INSTRUCCION_INVALIDA, desc);
    logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    return 0;
}

int rutinaManejadora_DireccionInvalida(void) {
    const char *desc = "Direccionamiento invalido (Violacion de Memoria)";
    logInterrupcion("Ciclo %d | Interrupcion %d (FATAL): %s", contadorCiclos, INT_DIRECCION_INVALIDA, desc);
    logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    return 0;
}

int rutinaManejadora_Underflow(void) {
    const char *desc = "Stack Underflow";
    logInterrupcion("Ciclo %d | Interrupcion %d (FATAL): %s", contadorCiclos, INT_UNDERFLOW, desc);
    logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    return 0;
}

int rutinaManejadora_Overflow(void) {
    const char *desc = "Overflow Aritmetico";
    logInterrupcion("Ciclo %d | Interrupcion %d (FATAL): %s", contadorCiclos, INT_OVERFLOW, desc);
    logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    return 0;
}

void inicializarVectorInterrupciones() {
    vectorInterrupciones[INT_SYSCALL_INVALIDA] = rutinaManejadora_SyscallInvalida;
    vectorInterrupciones[INT_CODIGO_INVALIDO] = rutinaManejadora_CodigoInvalido;
    vectorInterrupciones[INT_SVC] = rutinaManejadora_SVC;
    vectorInterrupciones[INT_TIMER] = rutinaManejadora_Timer;
    vectorInterrupciones[INT_IO_DONE] = rutinaManejadora_IODone;
    vectorInterrupciones[INT_INSTRUCCION_INVALIDA] = rutinaManejadora_InstruccionInvalida;
    vectorInterrupciones[INT_DIRECCION_INVALIDA] = rutinaManejadora_DireccionInvalida;
    vectorInterrupciones[INT_UNDERFLOW] = rutinaManejadora_Underflow;
    vectorInterrupciones[INT_OVERFLOW] = rutinaManejadora_Overflow;
}

// Nueva funcion que busca y procesa todas las banderas en la jerarquia (0 a 8)
int procesarInterrupcionesPendientes() {
    // Si no hay vector activo, salir
    if (vectorInterrupciones[0] == NULL) return 1;

    for (int i = 0; i < NUM_INTERRUPCIONES; i++) {
        if (interrupcionesPendientes[i]) {
            // Limpiarla antes de procesar
            interrupcionesPendientes[i] = 0;
            
            // hardware preparation
            guardarContexto();
            
            // Backup del PSW original antes de poner kernel
            Psw pswOriginal = registrosCpu.psw;
            
            registrosCpu.psw.modoOperacion = MODO_KERNEL;
            registrosCpu.psw.habilitarInterrupciones = INT_DESHABILITADAS;

            // Invocar la rutina apuntada por el vector (Análisis polinómico O(1))
            int esRecuperable = vectorInterrupciones[i]();

            // Salida de la subrutina
            if (esRecuperable == 0) {
                imprimirLog("Deteniendo ejecucion - Terminando programa");
                cpuEjecutando = 0;
                return 0; // Si fue fatal, terminamos de procesar
            } else if (esRecuperable == 1) {
                // Si fue recuperable y la interrupcion no saco al proceso de CPU
                registrosCpu.psw = pswOriginal; // Limpiamos el modo kernel antes de restaurar Pila
                restaurarContexto();
            } else if (esRecuperable == 2) {
                // S.O. decidio cambiar proceso, no restauramos el viejo.
                // Como Despachador salvó el BCP *con* modo Kernel si no se lo sacamos,
                // hay que asegurar que la proxima vez que este proceso despierte no corra en anillo 0.
                // Sin embargo el DespacharProceso ya salvo la copia. 
                // La solucion correcta se hara en DespacharProceso limpiando esa bandera si existe.
            }

            registrosCpu.psw.modoOperacion = MODO_USUARIO;
            registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
        }
    }
    return 1;
}

