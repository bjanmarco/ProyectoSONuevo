/*
 * ============================================================================
 * ARCHIVO: cpu.c
 * DESCRIPCION: Implementacion del CPU de la maquina virtual.
 *              Contiene el ciclo de instruccion y manejo de interrupciones.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/cpu.h"
#include "../include/hardware.h"
#include "../include/memoria.h"
#include "../include/disco.h"
#include "../include/dma.h"
#include "../include/logger.h"

// Bandera que indica si el CPU esta ejecutando
int cpuEjecutando = 0;

// Variables para deteccion de bucles infinitos (JMP .)
static int lastPC = -1;
static int repetitionCount = 0;

// Funcion para reiniciar la deteccion de bucles (llamada desde loader)
void reiniciarDeteccionBucle() {
    lastPC = -1;
    repetitionCount = 0;
}

// Contador de ciclos para interrupciones de reloj
int contadorCiclos = 0;


// Intervalo para generar interrupcion de reloj (0 = deshabilitado)
int intervaloReloj = 0;


// Bandera de interrupcion pendiente
int interrupcionPendiente = 0;

// Codigo de la interrupcion pendiente
int codigoInterrupcionPendiente = -1;

Registros registrosCpu;

int palabraAEntero(Palabra p) {
    int valor = p.digitos;
    if (p.signo == 1) {
        valor = -valor;  // Aplicar signo negativo
    }
    return valor;
}

/*
 * Convierte un entero con signo a una Palabra (signo-magnitud).
 */
Palabra enteroAPalabra(int val) {
    Palabra p;
    if (val < 0) {
        p.signo = 1;         // Negativo
        p.digitos = -val;    // Magnitud absoluta
    } else {
        p.signo = 0;         // Positivo
        p.digitos = val;
    }
    // Verificar overflow (maximo 8 digitos = 99999999)
    // Se requiere 8 digitos para instrucciones como JMP (27000000)
    if (p.digitos > 99999999) {
        p.digitos = 99999999;
        // Actualizar codigo de condicion a DESBORDAMIENTO (CC=3)
        registrosCpu.psw.codigoCondicion = CC_DESBORDAMIENTO;
        interrupcionPendiente = 1;
        codigoInterrupcionPendiente = INT_OVERFLOW;
    }
    return p;
}

// Muestra mensajes en el log (y consola si es critico/debug)
void imprimirLog(const char *mensaje) {
    // Usamos logCpu para que vaya al archivo .log
    // Se incluye el ciclo actual para contexto
    logCpu("[Ciclo %d] %s", contadorCiclos, mensaje);
}

void imprimirEstadoCpu() {
    printf("\n========== ESTADO DEL CPU ==========\n");
    printf("AC:  %s%07d\n", registrosCpu.ac.signo ? "-" : "+", registrosCpu.ac.digitos);
    printf("PC:  %05d (logico)\n", registrosCpu.psw.pc);
    printf("MAR: %s%07d\n", registrosCpu.mar.signo ? "-" : "+", registrosCpu.mar.digitos);
    printf("MDR: %s%07d\n", registrosCpu.mdr.signo ? "-" : "+", registrosCpu.mdr.digitos);
    printf("IR:  Op=%02d Dir=%d Val=%05d\n", 
           registrosCpu.ir.codigoOperacion,
           registrosCpu.ir.direccionamiento,
           registrosCpu.ir.valor);
    printf("RB:  %05d  RL: %05d\n", registrosCpu.rb, registrosCpu.rl);
    printf("RX:  %05d  SP: %05d\n", registrosCpu.rx, registrosCpu.sp);
    printf("PSW: CC=%d Modo=%s Int=%s\n",
           registrosCpu.psw.codigoCondicion,
           registrosCpu.psw.modoOperacion == MODO_KERNEL ? "KERNEL" : "USUARIO",
           registrosCpu.psw.habilitarInterrupciones ? "HAB" : "DESHAB");
    printf("=====================================\n\n");
}

int traducirDireccion(int direccionLogica) {
    if (registrosCpu.psw.modoOperacion == MODO_KERNEL) {
        return direccionLogica;
    }
    return direccionLogica + registrosCpu.rb;
}

/*
 * Verifica proteccion de memoria (solo en modo usuario).
 * Retorna 1 si es valida, 0 si viola proteccion.
 */
int verificarProteccionMemoria(int direccionFisica) {
    // En modo kernel no se verifica proteccion
    if (registrosCpu.psw.modoOperacion == MODO_KERNEL) {
        return 1;
    }
    // En modo usuario: RB <= direccion_fisica <= RL
    if (direccionFisica >= registrosCpu.rb && direccionFisica <= registrosCpu.rl) {
        return 1;
    }
    return 0;
}

int esInstruccionPrivilegiada(int opcode) {
    // Instrucciones de usuario
    if (opcode >= 0 && opcode <= 5) return 0;   // Aritmeticas y transferencia
    if (opcode >= 8 && opcode <= 13) return 0;  // Comparacion, saltos, SVC
    if (opcode >= 25 && opcode <= 27) return 0; // Pila y salto incondicional
    // Todo lo demas es privilegiado
    return 1;
}

int verificarDireccionSalto(int direccionLogica) {
    int direccionFisica;
    
    // En modo kernel no se verifica
    if (registrosCpu.psw.modoOperacion == MODO_KERNEL) {
        return 1;
    }
    
    // Traducir a direccion fisica
    direccionFisica = traducirDireccion(direccionLogica);
    
    // Verificar que este dentro de los limites del programa
    // RB <= direccion_fisica <= RL
    if (direccionFisica >= registrosCpu.rb && direccionFisica <= registrosCpu.rl) {
        return 1;
    }
    
    return 0;
}

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
    // Asegurar que el valor sea positivo
    if (valor < 0) {
        valor = -valor;
    }
    // Extraer campos (8 digitos: CCMIPPPP)
    registrosCpu.psw.pc = valor % 100000;  // Ultimos 5 digitos
    registrosCpu.psw.habilitarInterrupciones = (valor / 100000) % 10;  // 6to digito
    registrosCpu.psw.modoOperacion = (valor / 1000000) % 10;  // 7mo digito
    registrosCpu.psw.codigoCondicion = (valor / 10000000) % 10;  // 8vo digito
}

/*
 * Obtiene el operando segun el modo de direccionamiento.
 */
Palabra obtenerOperando(int modo, int valor) {
    Palabra operando;
    int direccionFisica;
    
    switch (modo) {
        case DIR_DIRECTO:
            // El valor es una direccion logica
            direccionFisica = traducirDireccion(valor);
            if (!verificarProteccionMemoria(direccionFisica)) {
                interrupcionPendiente = 1;
                codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                operando = enteroAPalabra(0);
                return operando;
            }
            operando = leerMemoria(direccionFisica);
            break;
            
        case DIR_INMEDIATO:
            // El valor es el dato directamente
            operando = enteroAPalabra(valor);
            break;
            
        case DIR_INDEXADO:
            // El valor es un indice desde AC
            direccionFisica = traducirDireccion(valor + palabraAEntero(registrosCpu.ac));
            if (!verificarProteccionMemoria(direccionFisica)) {
                interrupcionPendiente = 1;
                codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
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
    
    // Inicializar acumulador a 0
    registrosCpu.ac.signo = 0;
    registrosCpu.ac.digitos = 0;
    
    // Inicializar MAR y MDR a 0
    registrosCpu.mar.signo = 0;
    registrosCpu.mar.digitos = 0;
    registrosCpu.mdr.signo = 0;
    registrosCpu.mdr.digitos = 0;
    
    // Inicializar IR
    registrosCpu.ir.codigoOperacion = 0;
    registrosCpu.ir.direccionamiento = 0;
    registrosCpu.ir.valor = 0;
    
    // Inicializar registros de proteccion de memoria
    // NOTA: Estos se establecen cuando se carga un programa
    registrosCpu.rb = INICIO_MEMORIA_USUARIO;  // 300
    registrosCpu.rl = TAMANO_MEMORIA - 1;      // 1999
    
    // Inicializar pila al final de la memoria
    registrosCpu.rx = TAMANO_MEMORIA - 1;  // Base de pila
    registrosCpu.sp = TAMANO_MEMORIA - 1;  // Tope de pila
    
    // Inicializar PSW
    // El CPU inicia en modo KERNEL para la carga del sistema operativo
    // Luego cambiara a modo USUARIO cuando termine la inicializacion
    registrosCpu.psw.codigoCondicion = CC_CERO;
    registrosCpu.psw.modoOperacion = MODO_KERNEL;
    registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
    registrosCpu.psw.pc = 0;  // PC inicia en 0 (logico)
    
    // Reiniciar contadores
    contadorCiclos = 0;
    intervaloReloj = 0;
    interrupcionPendiente = 0;
    codigoInterrupcionPendiente = -1;
    cpuEjecutando = 0;
    
    // Reiniciar deteccion de bucles
    lastPC = -1;
    repetitionCount = 0;
    
    imprimirLog("CPU inicializado correctamente");
}

/*
 * Bucle principal del CPU. Ejecuta ciclos hasta fin de programa (segun RB/RL) o error fatal.
 */
void ejecutarCpu() {
    cpuEjecutando = 1;
    imprimirLog("Iniciando ejecucion del CPU");
    
    // Bucle principal de ejecucion
    while (cpuEjecutando) {
        // Ejecutar un ciclo de instruccion
        if (!cicloCpu()) {
            // cicloCpu retorno 0, detener ejecucion
            break;
        }
        

    }
    
    // Mostrar estado final
    
    // Mostrar estado final
    imprimirLog("Devolviendo control a la consola");
    // imprimirEstadoCpu(); // Deshabilitado: Usuario no desea ver registros al finalizar run
}

int cicloCpu() {
    char buffer[100];
    
    // Deteccion de bucle infinito (PC estatico)
    // Si el PC no cambia durante 5 ciclos, asumimos deadlock o JMP .
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
    
    // Antes de FETCH: verificar fin por RL inclusivo o PC fuera de RB
    int direccionFisicaPC = traducirDireccion(registrosCpu.psw.pc);
    // Si PC > RL -> fin del programa (RL es inclusivo)
    if (direccionFisicaPC > registrosCpu.rl) {
        imprimirLog("Detectada ultima instruccion, fin de programa");
        cpuEjecutando = 0;
        return 0;
    }
    // Si PC < RB -> direccion invalida
    if (direccionFisicaPC < registrosCpu.rb) {
        imprimirLog("ERROR: PC < RB - Direccion invalida");
        interrupcionPendiente = 1;
        codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
        return 1;
    }

    // 1. FETCH
    faseFetch();
    
    // 2. DECODE
    faseDecode();
    
    sprintf(buffer, "FETCH-DECODE: Op=%02d Dir=%d Val=%05d en PC=%05d",
            registrosCpu.ir.codigoOperacion,
            registrosCpu.ir.direccionamiento,
            registrosCpu.ir.valor,
            registrosCpu.psw.pc - 1);
    imprimirLog(buffer);
    
    // 3. EXECUTE
    faseExecute();
    
    // VERIFICACION DE INTERRUPCIONES (Parte final del ciclo)
    
    // 1. Interrupciones generadas por la instruccion (Pendientes/Fatales)
    if (interrupcionPendiente && registrosCpu.psw.habilitarInterrupciones) {
        // Intentar manejar la interrupcion
        if (!manejarInterrupcion(codigoInterrupcionPendiente)) {
            // Interrupcion FATAL -> Detener CPU
            cpuEjecutando = 0;
        }
        // Limpiar flags
        interrupcionPendiente = 0;
        codigoInterrupcionPendiente = -1;
    }
    
    // 2. Interrupcion de Reloj (Timer)
    if (intervaloReloj > 0) {
        contadorCiclos++;
        if (contadorCiclos >= intervaloReloj) {
            contadorCiclos = 0;
            if (registrosCpu.psw.habilitarInterrupciones) {
                manejarInterrupcion(INT_TIMER);
            }
        }
    }
    
    // 3. Interrupcion de E/S (DMA)
    if (verificarInterrupcionDma() && registrosCpu.psw.habilitarInterrupciones) {
        interrupcionPendienteDma = 0;
        manejarInterrupcion(INT_IO_DONE);
    }

    // Si hubo error fatal, cpuEjecutando sera 0
    return cpuEjecutando;
}

void faseFetch() {
    int direccionFisica;
    
    // Traducir PC (logico) a direccion fisica
    direccionFisica = traducirDireccion(registrosCpu.psw.pc);
    
    // MAR <- direccion fisica (la direccion real de memoria)
    registrosCpu.mar = enteroAPalabra(direccionFisica);
    
    // Verificar proteccion de memoria
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
    // Formato: [Signo][OODDVVVVV] donde OO=opcode (2d), D=modo (1d), VVVVV=valor (5d)
    int instruccionCompleta = registrosCpu.mdr.digitos;

    // Extraer campos de la instruccion (8 digitos totales)
    registrosCpu.ir.codigoOperacion = instruccionCompleta / 1000000;        // Primeros 2
    registrosCpu.ir.direccionamiento = (instruccionCompleta / 100000) % 10; // 3er digito
    registrosCpu.ir.valor = instruccionCompleta % 100000;                   // Ultimos 5
}

int faseExecute() {
    int opcode = registrosCpu.ir.codigoOperacion;
    int modo = registrosCpu.ir.direccionamiento;
    int valor = registrosCpu.ir.valor;
    Palabra operando;
    int resultado, valorAc, valorOp;
    int direccionFisica;
    char buffer[100];
    
    // Verificar instruccion privilegiada en modo usuario
    if (registrosCpu.psw.modoOperacion == MODO_USUARIO && esInstruccionPrivilegiada(opcode)) {
        sprintf(buffer, "ERROR: Instruccion privilegiada %02d en modo usuario", opcode);
        imprimirLog(buffer);
        interrupcionPendiente = 1;
        codigoInterrupcionPendiente = INT_INSTRUCCION_INVALIDA;
        return 1;
    }
    
    // Ejecutar segun opcode
    switch (opcode) {
        /* ===== GRUPO 1: ARITMETICAS (SUM, RES, MULT) ===== */
        case OP_SUM:
        case OP_RES:
        case OP_MULT:
            operando = obtenerOperando(modo, valor);
            valorAc = palabraAEntero(registrosCpu.ac);
            valorOp = palabraAEntero(operando);
            // Realizar operacion segun opcode
            if (opcode == OP_SUM) resultado = valorAc + valorOp;
            else if (opcode == OP_RES) resultado = valorAc - valorOp;
            else resultado = valorAc * valorOp;
            registrosCpu.ac = enteroAPalabra(resultado);
            actualizarCodigoCondicion(resultado);
            break;
            
        case OP_DIVI:  // 03: AC = AC / dato
            operando = obtenerOperando(modo, valor);
            valorOp = palabraAEntero(operando);
            if (valorOp == 0) {
                imprimirLog("ERROR: Division por cero");
                interrupcionPendiente = 1;
                codigoInterrupcionPendiente = INT_OVERFLOW;
                return 1;
            }
            valorAc = palabraAEntero(registrosCpu.ac);
            resultado = valorAc / valorOp;
            registrosCpu.ac = enteroAPalabra(resultado);
            actualizarCodigoCondicion(resultado);
            break;
            
        /* ===== GRUPO 2: TRANSFERENCIA AC-MEMORIA ===== */
        case OP_LOAD:  // 04: AC = M[direccion]
            operando = obtenerOperando(modo, valor);
            registrosCpu.ac = operando;
            break;
            
        case OP_STR:  // 05: M[direccion] = AC
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
                codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                return 1;
            }
            escribirMemoria(direccionFisica, registrosCpu.ac);
            break;
            
        /* ===== GRUPO 3: TRANSFERENCIA AC-REGISTROS (PRIVILEGIADAS) ===== */
        case OP_LOADRX:  // 06: AC = RX
            registrosCpu.ac = enteroAPalabra(registrosCpu.rx);
            break;
            
        case OP_STRRX:  // 07: RX = AC
            registrosCpu.rx = palabraAEntero(registrosCpu.ac);
            break;
            
        /* ===== GRUPO 4: COMPARACION Y SALTOS ===== */
        case OP_COMP:  // 08: Compara AC con dato
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
            
        /* ===== GRUPO 4: SALTOS CONDICIONALES (comparan AC con M[SP]) ===== */
        case OP_JMPE:
        case OP_JMPNE:
        case OP_JMPLT:
        case OP_JMPLGT:
            valorAc = palabraAEntero(registrosCpu.ac);
            valorOp = palabraAEntero(leerMemoria(registrosCpu.sp));
            // Evaluar condicion segun opcode
            int saltar = 0;
            if (opcode == OP_JMPE && valorAc == valorOp) saltar = 1;
            else if (opcode == OP_JMPNE && valorAc != valorOp) saltar = 1;
            else if (opcode == OP_JMPLT && valorAc < valorOp) saltar = 1;
            else if (opcode == OP_JMPLGT && valorAc > valorOp) saltar = 1;
            // Ejecutar salto si corresponde
            if (saltar) {
                if (!verificarDireccionSalto(valor)) {
                    imprimirLog("ERROR: Salto fuera de limites RB/RL");
                    interrupcionPendiente = 1;
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.psw.pc = valor;
            }
            break;
            
        /* ===== GRUPO 5: CONTROL DEL SISTEMA ===== */
        case OP_SVC:  // 13: Llamada al sistema
            imprimirLog("SVC: Llamada al sistema");
            interrupcionPendiente = 1;
            codigoInterrupcionPendiente = INT_SVC;
            break;
            
        case OP_RETRN:  // 14: Retorno de subrutina (PRIVILEGIADA)
            // Pop PC de la pila (verificar underflow)
            if (registrosCpu.sp >= registrosCpu.rx) {
                imprimirLog("ERROR: Stack underflow al hacer RETRN");
                interrupcionPendiente = 1;
                codigoInterrupcionPendiente = INT_UNDERFLOW;
                return 1;
            }
            registrosCpu.psw.pc = palabraAEntero(leerMemoria(registrosCpu.sp));
            registrosCpu.sp++;
            break;
            
        case OP_HAB:  // 15: Habilitar interrupciones (PRIVILEGIADA)
            registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
            imprimirLog("Interrupciones habilitadas");
            break;
            
        case OP_DHAB:  // 16: Deshabilitar interrupciones (PRIVILEGIADA)
            registrosCpu.psw.habilitarInterrupciones = INT_DESHABILITADAS;
            imprimirLog("Interrupciones deshabilitadas");
            break;
            
        case OP_TTI:  // 17: Establece intervalo de reloj (PRIVILEGIADA)
            intervaloReloj = valor;
            contadorCiclos = 0;
            sprintf(buffer, "Timer configurado: interrupcion cada %d ciclos", valor);
            imprimirLog(buffer);
            break;
            
        case OP_CHMOD:  // 18: Cambiar modo (PRIVILEGIADA)
            if (registrosCpu.psw.modoOperacion == MODO_KERNEL) {
                registrosCpu.psw.modoOperacion = MODO_USUARIO;
                imprimirLog("Cambio a modo USUARIO");
            } else {
                // Solo se puede cambiar a kernel desde kernel
                imprimirLog("ERROR: No se puede cambiar a kernel desde usuario");
            }
            break;
            
        /* ===== GRUPO 6: REGISTROS BASE/LIMITE/PILA (PRIVILEGIADAS) ===== */
        case OP_LOADRB:  // 19: AC = RB
        case OP_LOADRL:  // 21: AC = RL
        case OP_LOADSP:  // 23: AC = SP
            if (opcode == OP_LOADRB) registrosCpu.ac = enteroAPalabra(registrosCpu.rb);
            else if (opcode == OP_LOADRL) registrosCpu.ac = enteroAPalabra(registrosCpu.rl);
            else registrosCpu.ac = enteroAPalabra(registrosCpu.sp);
            break;
            
        case OP_STRRB:  // 20: RB = AC
            {
                int nuevoRB = palabraAEntero(registrosCpu.ac);
                if (nuevoRB < INICIO_MEMORIA_USUARIO || nuevoRB >= registrosCpu.rl) {
                    imprimirLog("ERROR: Valor invalido para RB");
                    interrupcionPendiente = 1;
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.rb = nuevoRB;
            }
            break;
            
        case OP_STRRL:  // 22: RL = AC
            {
                int nuevoRL = palabraAEntero(registrosCpu.ac);
                if (nuevoRL < registrosCpu.rb || nuevoRL >= TAMANO_MEMORIA) {
                    imprimirLog("ERROR: Valor invalido para RL");
                    interrupcionPendiente = 1;
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.rl = nuevoRL;
            }
            break;
            
        case OP_STRSP:  // 24: SP = AC
            {
                int nuevoSP = palabraAEntero(registrosCpu.ac);
                if (nuevoSP < registrosCpu.rb || nuevoSP > registrosCpu.rx) {
                    imprimirLog("ERROR: Valor invalido para SP");
                    interrupcionPendiente = 1;
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.sp = nuevoSP;
            }
            break;
            
        /* ===== GRUPO 7: PILA ===== */
        case OP_PSH:  // 25: Push AC a pila
            registrosCpu.sp--;
            if (registrosCpu.sp < registrosCpu.rb) {
                imprimirLog("ERROR: Stack overflow");
                interrupcionPendiente = 1;
                codigoInterrupcionPendiente = INT_OVERFLOW;
                return 1;
            }
            escribirMemoria(registrosCpu.sp, registrosCpu.ac);
            break;
            
        case OP_POP:  // 26: Pop de pila a AC
            // Underflow: SP == RX => pila vacia
            if (registrosCpu.sp >= registrosCpu.rx) {
                imprimirLog("ERROR: Stack underflow");
                interrupcionPendiente = 1;
                codigoInterrupcionPendiente = INT_UNDERFLOW;
                return 1;
            }
            registrosCpu.ac = leerMemoria(registrosCpu.sp);
            registrosCpu.sp++;
            break; 
            
        /* ===== GRUPO 8: SALTO INCONDICIONAL ===== */
        case OP_J:  // 27: PC = direccion (salto incondicional)
            // Verificar que la direccion destino este dentro de limites
            if (!verificarDireccionSalto(valor)) {
                imprimirLog("ERROR: Salto fuera de limites RB/RL");
                interrupcionPendiente = 1;
                codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                return 1;
            }
            registrosCpu.psw.pc = valor;
            break;
            
        /* ===== GRUPO 9: DMA (Instrucciones de E/S) ===== */
        case OP_SDMAP:  // 28: Establecer pista del DMA
            dma.pistaSeleccionada = valor;
            sprintf(buffer, "DMA: Pista establecida a %d", valor);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAC:  // 29: Establecer cilindro del DMA
            dma.cilindroSeleccionado = valor;
            sprintf(buffer, "DMA: Cilindro establecido a %d", valor);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAS:  // 30: Establecer sector del DMA
            dma.sectorSeleccionado = valor;
            sprintf(buffer, "DMA: Sector establecido a %d", valor);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAIO:  // 31: Establecer direccion de E/S (0=leer, 1=escribir)
            dma.direccionIo = valor;
            sprintf(buffer, "DMA: Direccion E/S establecida a %d (%s)",
                    valor, valor == 0 ? "LEER" : "ESCRIBIR");
            imprimirLog(buffer);
            break;
            
        case OP_SDMAM:  // 32: Establecer direccion de memoria
            dma.direccionMemoria = valor;
            sprintf(buffer, "DMA: Direccion de memoria establecida a %d", valor);
            imprimirLog(buffer);
            break;
            
        case OP_SDMAON:  // 33: Iniciar transferencia DMA
            imprimirLog("DMA: Iniciando transferencia...");
            iniciarTransferenciaDma();
            break;
            
        default:
            sprintf(buffer, "ERROR: Opcode invalido %02d", opcode);
            imprimirLog(buffer);
            interrupcionPendiente = 1;
            codigoInterrupcionPendiente = INT_INSTRUCCION_INVALIDA;
            return 1;
    }
    
    return 1;  // Continuar ejecucion
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
    
    // Apilar registros: AC, RB, RL, RX, SP original, PSW
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
    
    // Desapilar en orden inverso: PSW, SP original, RX, RL, RB, AC
    decodificarPsw(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
    int spOriginal = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
    registrosCpu.rx = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
    registrosCpu.rl = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
    registrosCpu.rb = palabraAEntero(leerMemoria(registrosCpu.sp)); registrosCpu.sp++;
    registrosCpu.ac = leerMemoria(registrosCpu.sp); registrosCpu.sp++;
    
    // Restaurar SP al valor original
    registrosCpu.sp = spOriginal;
    imprimirLog("Contexto restaurado exitosamente");
}

/*
 * Maneja una interrupcion. Retorna 1 si es recuperable, 0 si es fatal.
 */
int manejarInterrupcion(int codigoInterrupcion) {

    int esRecuperable = 0;
    
    // Modificado para usar sistema de logs
    // sprintf(buffer, "=== INTERRUPCION %d ===", codigoInterrupcion);
    // imprimirLog(buffer);
    // printf("[INTERRUPCION] Codigo: %d\n", codigoInterrupcion);
    
    // 1. Guardar contexto
    guardarContexto();
    
    // 2. Cambiar a modo kernel
    registrosCpu.psw.modoOperacion = MODO_KERNEL;
    
    // 3. Deshabilitar interrupciones
    registrosCpu.psw.habilitarInterrupciones = INT_DESHABILITADAS;
    
    // 4. Determinar si es recuperable y ejecutar manejador
    const char *desc = "Desconocida";
    
    // 4. Determinar si es recuperable y obtener descripcion
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

    // Mensaje unificado y claro tanto para consola como log
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
            
    // (Bloque de casos antiguos eliminado)
    
    // 5. Si es recuperable, restaurar contexto y volver a modo usuario
    if (esRecuperable) {
        restaurarContexto();
        imprimirLog("Retornando de interrupcion");
    } else {
        imprimirLog("Deteniendo ejecucion - Terminando programa");
        cpuEjecutando = 0;
    }
    
    registrosCpu.psw.modoOperacion = MODO_USUARIO;
    registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
    return esRecuperable;
}

