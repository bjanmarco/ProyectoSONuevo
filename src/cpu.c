// Simulacion del Procesador Central (CPU)
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

int cpuEjecutando = 0;

// Variables para deteccion de bucles infinitos
static int lastPC = -1;
static int repetitionCount = 0;

void reiniciarDeteccionBucle() {
    lastPC = -1;
    repetitionCount = 0;
}

// Manejo de Interrupciones y Reloj
int contadorCiclos = 0;
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

// Convertir entero con signo a Palabra
Palabra enteroAPalabra(int val) {
    Palabra p;
    if (val < 0) {
        p.signo = 1;
        p.digitos = -val;
    } else {
        p.signo = 0;
        p.digitos = val;
    }
    // Verificar overflow 
    if (p.digitos > 99999999) {
        p.digitos = 99999999;
        // Actualizar CC_DESBORDAMIENTO
        registrosCpu.psw.codigoCondicion = CC_DESBORDAMIENTO;
        interrupcionPendiente = 1;
        interrupcionesPendientes[INT_OVERFLOW] = 1;
    }
    return p;
}

// Emitir Log de CPU
void imprimirLog(const char *mensaje) {
    logCpu("[Ciclo %d] %s", contadorCiclos, mensaje);
}

int traducirDireccion(int direccionLogica) {
    // Aún en MODO_KERNEL sumamos el RB del proceso.
    // El Modo Kernel otorga privilegios de ejecución pero el programa
    // cargado sigue estando relativo a su Base.
    return direccionLogica + registrosCpu.rb;
}

// Verificacion de proteccion de memoria (Modo Usuario)
int verificarProteccionMemoria(int direccionFisica) {
    if (registrosCpu.psw.modoOperacion == MODO_KERNEL) {
        return 1;
    }
    
    // Regla de Fase 2: Pila mediante PSH/POP unicamente. 
    // Proteccion 1: No bajar del Registro Base 
    if (direccionFisica < registrosCpu.rb) {
        return 0;
    }
    
    // Proteccion 2: No tocar ni exceder la Pila del Proceso 
    if (direccionFisica >= registrosCpu.sp) {
        return 0;
    }
    
    return 1;
}

int esInstruccionPrivilegiada(int opcode) {
    if (opcode == 15 || opcode == 16 || opcode == 17 || opcode == 18) {
        return 1;
    }
    if (opcode >= 28 && opcode <= 33) {
        return 1;
    }
    
    return 0;
}

int verificarDireccionSalto(int direccionLogica) {
    int direccionFisica;
    
    if (registrosCpu.psw.modoOperacion == MODO_KERNEL) {
        return 1; 
    }
    
    direccionFisica = traducirDireccion(direccionLogica);
    
    // Verificar limites
    if (direccionFisica >= registrosCpu.rb && direccionFisica <= registrosCpu.rl) {
        return 1;
    }
    
    return 0;
}

// Actualizar CC
void actualizarCodigoCondicion(int resultado) {
    if (resultado > 9999999 || resultado < -9999999) {
        registrosCpu.psw.codigoCondicion = CC_DESBORDAMIENTO;
    } else if (resultado == 0) {
        registrosCpu.psw.codigoCondicion = CC_CERO;
    } else if (resultado < 0) {
        registrosCpu.psw.codigoCondicion = CC_NEGATIVO;
    } else {
        registrosCpu.psw.codigoCondicion = CC_POSITIVO;
    }
}

Palabra codificarPsw() {
    int valor = registrosCpu.psw.codigoCondicion * 10000000 + 
                registrosCpu.psw.modoOperacion * 1000000 + 
                registrosCpu.psw.habilitarInterrupciones * 100000 + 
                registrosCpu.psw.pc; 
    return enteroAPalabra(valor);
}

void decodificarPsw(Palabra pswPalabra) {
    int valor = palabraAEntero(pswPalabra);
    if (valor < 0) {
        valor = -valor;
    }
    registrosCpu.psw.pc = valor % 100000;  
    registrosCpu.psw.habilitarInterrupciones = (valor / 100000) % 10;
    registrosCpu.psw.modoOperacion = (valor / 1000000) % 10;
    registrosCpu.psw.codigoCondicion = (valor / 10000000) % 10;
}

// Obtener operando
Palabra obtenerOperando(int modo, int valor) {
    Palabra operando;
    int direccionFisica;
    
    switch (modo) {
        case DIR_DIRECTO:
            direccionFisica = traducirDireccion(valor);
            if (!verificarProteccionMemoria(direccionFisica)) {
                interrupcionPendiente = 1; 
                interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
                operando = enteroAPalabra(0);
                return operando;
            }
            operando = leerMemoria(direccionFisica);
            break;
            
        case DIR_INMEDIATO:
            operando = enteroAPalabra(valor);
            if (registrosCpu.mdr.signo == 1) {
                operando.signo = 1;
            }
            break;
            
        case DIR_INDEXADO:
            direccionFisica = traducirDireccion(valor + palabraAEntero(registrosCpu.ac));
            if (!verificarProteccionMemoria(direccionFisica)) {
                interrupcionPendiente = 1;
                interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
                operando = enteroAPalabra(0);
                return operando;
            }
            operando = leerMemoria(direccionFisica);
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
    
    registrosCpu.rb = INICIO_MEMORIA_USUARIO;  // 300
    registrosCpu.rl = TAMANO_MEMORIA - 1;      // 1999
    
    registrosCpu.rx = TAMANO_MEMORIA - 1;  
    registrosCpu.sp = TAMANO_MEMORIA - 1;  
    
    // El CPU inicia en MODO_KERNEL
    registrosCpu.psw.codigoCondicion = CC_CERO;
    registrosCpu.psw.modoOperacion = MODO_KERNEL;
    registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
    registrosCpu.psw.pc = 0;
    
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

// Bucle de ejecucion principal de CPU
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
    
    // Deteccion de bucle infinito 
    if (registrosCpu.psw.pc == lastPC) {
        repetitionCount++;
        if (repetitionCount >= 5) {
            imprimirLog("ALERTA: Posible bucle infinito detectado (PC estatico). Deteniendo ejecucion.");
            cpuEjecutando = 0;
            return 0;
        }
    } else {
        lastPC = registrosCpu.psw.pc;
        repetitionCount = 0;
    }
    
    // Verificar fin por RL
    int direccionFisicaPC = traducirDireccion(registrosCpu.psw.pc);
    if (direccionFisicaPC > registrosCpu.rl) {
        sprintf(buffer, "DEBUG: Fin por RL superado. ProcID: %d, PC Logico: %d, DirFisica: %d, RL: %d, SP: %d", procesoEnEjecucion, registrosCpu.psw.pc, direccionFisicaPC, registrosCpu.rl, registrosCpu.sp);
        imprimirLog(buffer);
        imprimirLog("Detectada ultima instruccion, fin de programa");
        guardarContexto();
        cpuEjecutando = 0;
        return 0;
    }
    
    // Verificacion de instruccion NULA
    Palabra probeStr = leerMemoria(direccionFisicaPC);
    if (palabraAEntero(probeStr) == 0 && probeStr.signo == 0) {
        sprintf(buffer, "DEBUG: Fin por instruccion NULA. ProcID: %d, PC Logico: %d, DirFisica: %d", procesoEnEjecucion, registrosCpu.psw.pc, direccionFisicaPC);
        imprimirLog(buffer);
        imprimirLog("Detectada ultima instruccion, fin de programa");
        guardarContexto();
        cpuEjecutando = 0;
        return 0;
    }

    if (direccionFisicaPC < registrosCpu.rb) {
        imprimirLog("ERROR: PC < RB - Direccion invalida");
        interrupcionPendiente = 1;
        interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
        return 1;
    }

    // FASE FETCH
    faseFetch();
    
    // FASE DECODE
    faseDecode();
    
    sprintf(buffer, "FETCH-DECODE: Op=%02d Dir=%d Val=%05d en PC=%05d",
            registrosCpu.ir.codigoOperacion,
            registrosCpu.ir.direccionamiento,
            registrosCpu.ir.valor,
            registrosCpu.psw.pc - 1);
    imprimirLog(buffer);
    
    // FASE EXECUTE
    faseExecute();
    
    // interrupcion de reloj
    if (intervaloReloj > 0) {
        contadorCiclos++;
        actualizarProcesosDormidos();
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

    // Analisis de interrupciones pendientes
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
    
    // Traducir PC logico a direccion fisica
    direccionFisica = traducirDireccion(registrosCpu.psw.pc);
    
    // MAR <- direccion fisica
    registrosCpu.mar = enteroAPalabra(direccionFisica);
    
    // Verificar proteccion de memoria
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
    // FASE DECODE: Extraer campos desde MDR y colocarlos en IR
    int instruccionCompleta = registrosCpu.mdr.digitos;

    registrosCpu.ir.codigoOperacion = instruccionCompleta / 1000000;
    registrosCpu.ir.direccionamiento = (instruccionCompleta / 100000) % 10;
    registrosCpu.ir.valor = instruccionCompleta % 100000;
}

int faseExecute() {
    int opcode = registrosCpu.ir.codigoOperacion;
    int modo = registrosCpu.ir.direccionamiento;
    int valor = registrosCpu.ir.valor;
    
    Palabra operando;
    int resultado, valorAc, valorOp;
    int direccionFisica;
    char buffer[100];
    
    // Verificar si la instruccion es privilegiada en modo usuario
    if (registrosCpu.psw.modoOperacion == MODO_USUARIO && esInstruccionPrivilegiada(opcode)) {
        sprintf(buffer, "ERROR: Instruccion privilegiada %02d en modo usuario", opcode);
        imprimirLog(buffer);
        interrupcionPendiente = 1;
        interrupcionesPendientes[INT_INSTRUCCION_INVALIDA] = 1;
        return 1;
    }
    // Ejecutar segun opcode
    switch (opcode) {
        case OP_SUM:
        case OP_RES:
        case OP_MULT:
            operando = obtenerOperando(modo, valor);
            valorAc = palabraAEntero(registrosCpu.ac);
            valorOp = palabraAEntero(operando);
            if (opcode == OP_SUM) {
                resultado = valorAc + valorOp;
                sprintf(buffer, "SUM: %d + %d = %d", valorAc, valorOp, resultado);
                imprimirLog(buffer);
            }
            else if (opcode == OP_RES) {
                resultado = valorAc - valorOp;
                sprintf(buffer, "RES: %d - %d = %d", valorAc, valorOp, resultado);
                imprimirLog(buffer);
            }
            else {
                resultado = valorAc * valorOp;
                sprintf(buffer, "MULT: %d * %d = %d", valorAc, valorOp, resultado);
                imprimirLog(buffer);
            }
            registrosCpu.ac = enteroAPalabra(resultado);
            actualizarCodigoCondicion(resultado);
            break;
            
        case OP_DIVI:
            operando = obtenerOperando(modo, valor);
            valorOp = palabraAEntero(operando);
            if (valorOp == 0) {
                imprimirLog("ERROR: Division por cero");
                interrupcionPendiente = 1;
                interrupcionesPendientes[INT_OVERFLOW] = 1;
                return 1;
            }
            valorAc = palabraAEntero(registrosCpu.ac);
            resultado = valorAc / valorOp;
            registrosCpu.ac = enteroAPalabra(resultado);
            actualizarCodigoCondicion(resultado);
            break;
            
        case OP_LOAD:
            operando = obtenerOperando(modo, valor);
            registrosCpu.ac = operando;
            break;
            
        case OP_STR:
            if (modo == DIR_INMEDIATO) {
                imprimirLog("ERROR: STR no soporta modo inmediato");
                return 1;
            }
            if (modo == DIR_INDEXADO) {
                direccionFisica = traducirDireccion(valor + palabraAEntero(registrosCpu.ac));
            } else {
                direccionFisica = traducirDireccion(valor);
            }
            if (!verificarProteccionMemoria(direccionFisica)) {
                interrupcionPendiente = 1;
                interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
                return 1;
            }
            escribirMemoria(direccionFisica, registrosCpu.ac);
            break;
            
        case OP_LOADRX:
            registrosCpu.ac = enteroAPalabra(registrosCpu.rx);
            break;
            
        case OP_STRRX:
            registrosCpu.rx = palabraAEntero(registrosCpu.ac);
            break;
            
        case OP_COMP:
            operando = obtenerOperando(modo, valor);
            valorAc = palabraAEntero(registrosCpu.ac);
            valorOp = palabraAEntero(operando);
            if (valorAc == valorOp) {
                registrosCpu.psw.codigoCondicion = CC_CERO;
            } else if (valorAc < valorOp) {
                registrosCpu.psw.codigoCondicion = CC_NEGATIVO;
            } else {
                registrosCpu.psw.codigoCondicion = CC_POSITIVO;
            }
            break;
            
        case OP_JMPE:
        case OP_JMPNE:
        case OP_JMPLT:
        case OP_JMPLGT:
            valorAc = palabraAEntero(registrosCpu.ac);
            Palabra palabraPila = leerMemoria(registrosCpu.sp);
            valorOp = palabraAEntero(palabraPila);
            
            int saltar = 0;
            if (opcode == OP_JMPE && valorAc == valorOp) saltar = 1;
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
            
        case OP_TTI:
            intervaloReloj = valor;
            contadorCiclos = 0;
            sprintf(buffer, "Timer configurado: interrupcion cada %d ciclos", valor);
            imprimirLog(buffer);
            break;
            
        case OP_CHMOD:
            if (registrosCpu.psw.modoOperacion == MODO_KERNEL) {
                registrosCpu.psw.modoOperacion = MODO_USUARIO;
                imprimirLog("Cambio a modo USUARIO");
            } else {
                imprimirLog("ERROR: No se puede cambiar a kernel desde usuario");
            }
            break;
            
        case OP_LOADRB:
        case OP_LOADRL:
        case OP_LOADSP:
            if (opcode == OP_LOADRB) registrosCpu.ac = enteroAPalabra(registrosCpu.rb);
            else if (opcode == OP_LOADRL) registrosCpu.ac = enteroAPalabra(registrosCpu.rl);
            else registrosCpu.ac = enteroAPalabra(registrosCpu.sp);
            break;
            
        case OP_STRRB:
            {
                int nuevoRB = palabraAEntero(registrosCpu.ac);
                if (nuevoRB < INICIO_MEMORIA_USUARIO || nuevoRB >= registrosCpu.rl) {
                    imprimirLog("ERROR: Valor invalido para RB");
                    interrupcionPendiente = 1;
                    interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
                    return 1;
                }
                registrosCpu.rb = nuevoRB;
            }
            break;
            
        case OP_STRRL:
            {
                int nuevoRL = palabraAEntero(registrosCpu.ac);
                if (nuevoRL < registrosCpu.rb || nuevoRL >= TAMANO_MEMORIA) {
                    imprimirLog("ERROR: Valor invalido para RL");
                    interrupcionPendiente = 1;
                    interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
                    return 1;
                }
                registrosCpu.rl = nuevoRL;
            }
            break;
            
        case OP_STRSP:
            {
                int nuevoSP = palabraAEntero(registrosCpu.ac);
                if (nuevoSP < registrosCpu.rb || nuevoSP > registrosCpu.rx) {
                    imprimirLog("ERROR: Valor invalido para SP");
                    interrupcionPendiente = 1;
                    interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
                    return 1;
                }
                registrosCpu.sp = nuevoSP;
            }
            break;
            
        case OP_PSH:
            registrosCpu.sp--;
            if (registrosCpu.sp < registrosCpu.rb) {
                imprimirLog("ERROR: Stack overflow");
                interrupcionPendiente = 1; 
                interrupcionesPendientes[INT_OVERFLOW] = 1;
                return 1;
            }
            if (modo != 0 || valor != 0) {
                operando = obtenerOperando(modo, valor);
                escribirMemoria(registrosCpu.sp, operando);
                sprintf(buffer, "PSH: Apilando operando %d", palabraAEntero(operando));
                imprimirLog(buffer);
            } else {
                escribirMemoria(registrosCpu.sp, registrosCpu.ac);
                sprintf(buffer, "PSH: Apilando AC (%d)", palabraAEntero(registrosCpu.ac));
                imprimirLog(buffer);
            }
            break;
            
        case OP_POP:
            if (registrosCpu.sp > registrosCpu.rx) {
                imprimirLog("ERROR: Stack underflow");
                interrupcionPendiente = 1;
                interrupcionesPendientes[INT_UNDERFLOW] = 1;
                return 1;
            }
            registrosCpu.ac = leerMemoria(registrosCpu.sp);
            registrosCpu.sp++;
            break; 
            
        case OP_J:
            if (!verificarDireccionSalto(valor)) {
                imprimirLog("ERROR: Salto fuera de limites RB/RL");
                interrupcionPendiente = 1;
                interrupcionesPendientes[INT_DIRECCION_INVALIDA] = 1;
                return 1;
            }
            registrosCpu.psw.pc = valor;
            break;
            
        case OP_SDMAP:
            dma.pistaSeleccionada = valor;
            sprintf(buffer, "DMA: Pista establecida a %d", valor);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAC:
            dma.cilindroSeleccionado = valor;
            sprintf(buffer, "DMA: Cilindro establecido a %d", valor);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAS:
            dma.sectorSeleccionado = valor;
            sprintf(buffer, "DMA: Sector establecido a %d", valor);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAIO:
            dma.direccionIo = valor;
            sprintf(buffer, "DMA: Direccion E/S establecida a %d (%s)",
                    valor, valor == 0 ? "LEER" : "ESCRIBIR");
            imprimirLog(buffer);
            break;
            
        case OP_SDMAM:
            dma.direccionMemoria = traducirDireccion(valor);
            sprintf(buffer, "DMA: Direccion de memoria de %d logica asignada a la %d fisica", valor, dma.direccionMemoria);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAON:
            imprimirLog("DMA: Iniciando transferencia...");
            iniciarTransferenciaDma();
            while (dma.ocupado) {
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
    logInterrupcion("Ciclo %d | ProcID %d | Interrupcion %d (FATAL): %s", contadorCiclos, procesoEnEjecucion, INT_SYSCALL_INVALIDA, desc);
    logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    return 0; // fatal
}

int rutinaManejadora_CodigoInvalido(void) {
    const char *desc = "Codigo de interrupcion invalido";
    logInterrupcion("Ciclo %d | ProcID %d | Interrupcion %d (FATAL): %s", contadorCiclos, procesoEnEjecucion, INT_CODIGO_INVALIDO, desc);
    logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    return 0; // fatal
}

int rutinaManejadora_SVC(void) {
    const char *desc = "Llamada al sistema (SVC)";
    logInterrupcion("Ciclo %d | ProcID %d | Interrupcion %d (RECUPERABLE): %s", contadorCiclos, procesoEnEjecucion, INT_SVC, desc);
    
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
            }
            // Limpiar el salto de linea que deja scanf en el buffer para evitar doble prompt en main
            int c;
            while((c = getchar()) != '\n' && c != EOF);
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
    logInterrupcion("Ciclo %d | ProcID %d | Interrupcion %d (RECUPERABLE): %s", contadorCiclos, procesoEnEjecucion, INT_TIMER, desc);
    
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
    logInterrupcion("Ciclo %d | ProcID %d | Interrupcion %d (RECUPERABLE): %s", contadorCiclos, procesoEnEjecucion, INT_IO_DONE, desc);
    logCpu("Manejando interrupcion recuperable: %s", desc);
    imprimirLog("Retornando de interrupcion");
    return 1; // recuperable
}

int rutinaManejadora_InstruccionInvalida(void) {
    const char *desc = "Instruccion invalida o privilegiada";
    logInterrupcion("Ciclo %d | ProcID %d | Interrupcion %d (FATAL): %s", contadorCiclos, procesoEnEjecucion, INT_INSTRUCCION_INVALIDA, desc);
    logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    return 0;
}

int rutinaManejadora_DireccionInvalida(void) {
    const char *desc = "Direccionamiento invalido (Violacion de Memoria)";
    logInterrupcion("Ciclo %d | ProcID %d | Interrupcion %d (FATAL): %s", contadorCiclos, procesoEnEjecucion, INT_DIRECCION_INVALIDA, desc);
    logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    return 0;
}

int rutinaManejadora_Underflow(void) {
    const char *desc = "Stack Underflow";
    logInterrupcion("Ciclo %d | ProcID %d | Interrupcion %d (FATAL): %s", contadorCiclos, procesoEnEjecucion, INT_UNDERFLOW, desc);
    logCpu("Deteniendo ejecucion debido a interrupcion fatal: %s", desc);
    return 0;
}

int rutinaManejadora_Overflow(void) {
    const char *desc = "Overflow Aritmetico";
    logInterrupcion("Ciclo %d | ProcID %d | Interrupcion %d (FATAL): %s", contadorCiclos, procesoEnEjecucion, INT_OVERFLOW, desc);
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
                // El Despachador ya cargó el contexto íntegro del nuevo proceso (incluyendo su PSW).
            }
        }
    }
    return 1;
}

