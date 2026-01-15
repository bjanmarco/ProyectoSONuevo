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

/* ============================================================================
 * VARIABLES GLOBALES DEL CPU
 * ============================================================================ */

// Bandera que indica si el CPU esta ejecutando
int cpuEjecutando = 0;

// Contador de ciclos para interrupciones de reloj
int contadorCiclos = 0;

// Intervalo para generar interrupcion de reloj (0 = deshabilitado)
int intervaloReloj = 0;

// Bandera de interrupcion pendiente
int interrupcionPendiente = 0;

// Codigo de la interrupcion pendiente
int codigoInterrupcionPendiente = -1;

// Contexto guardado para restaurar despues de interrupcion
// Ya no usamos un contexto guardado global; las funciones usan el paso por referencia.

/* ============================================================================
 * NOTA: La memoria se define en memoria.c y se accede via leerMemoria()
 * y escribirMemoria() para garantizar el arbitraje del bus.
 * ============================================================================ */

// Registros del CPU (definidos en hardware.h como extern)
Registros registrosCpu;

/* ============================================================================
 * FUNCIONES DE CONVERSION
 * ============================================================================ */

/*
 * Convierte una Palabra (signo-magnitud) a un entero con signo.
 */
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
        interrupcionPendiente = 1;
        codigoInterrupcionPendiente = INT_OVERFLOW;
    }
    return p;
}

/* ============================================================================
 * FUNCIONES AUXILIARES
 * ============================================================================ */

/*
 * Imprime un mensaje de log con informacion del ciclo actual.
 */
void imprimirLog(const char *mensaje) {
    printf("[CPU][Ciclo %d] %s\n", contadorCiclos, mensaje);
}

/*
 * Imprime el estado actual de todos los registros del CPU.
 */
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

/*
 * Verifica si una instruccion es el centinela de fin de programa.
 */
int esCentinela(Palabra instruccion) {
    // El centinela puede representarse como el valor maximo 99999999
    // o como un opcode 99 en la porcion de opcode. Comprobamos ambas.
    if (instruccion.digitos == 99999999) return 1;
    // Extraer posible opcode (si la instruccion tiene formato compacto)
    if (instruccion.digitos / 1000000 == 99) return 1;
    return 0;
}

/*
 * Traduce una direccion logica a fisica usando el Registro Base (RB).
 * direccion_fisica = direccion_logica + RB
 */
int traducirDireccion(int direccionLogica) {
    // Si estamos en modo kernel, la direccion ya podria ser fisica o 
    // relativa a 0 (inicio de memoria).
    // Asumiremos que en modo kernel, logica = fisica (directa)
    // OJO: Segun especificacion, "Toda direccion... relativa al proceso".
    // Pero en Kernel (OS) accedemos a todo.
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

/*
 * Verifica si un opcode es una instruccion privilegiada.
 * Instrucciones de usuario: 00-05, 08-13, 25-27
 * El resto son privilegiadas.
 */
int esInstruccionPrivilegiada(int opcode) {
    // Instrucciones de usuario
    if (opcode >= 0 && opcode <= 5) return 0;   // Aritmeticas y transferencia
    if (opcode >= 8 && opcode <= 13) return 0;  // Comparacion, saltos, SVC
    if (opcode >= 25 && opcode <= 27) return 0; // Pila y salto incondicional
    // Todo lo demas es privilegiado
    return 1;
}

/*
 * Verifica si una direccion de salto (logica) esta dentro de los limites.
 * La direccion logica debe estar entre 0 y (RL - RB) para ser valida.
 * Retorna 1 si es valida, 0 si esta fuera de limites.
 */
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

/*
 * Actualiza el codigo de condicion del PSW segun el resultado.
 */
void actualizarCodigoCondicion(int resultado) {
    if (resultado == 0) {
        registrosCpu.psw.codigoCondicion = CC_CERO;
    } else if (resultado < 0) {
        registrosCpu.psw.codigoCondicion = CC_NEGATIVO;
    } else {
        registrosCpu.psw.codigoCondicion = CC_POSITIVO;
    }
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

/* ============================================================================
 * FUNCIONES DE INICIALIZACION
 * ============================================================================ */

/*
 * Inicializa todos los registros del CPU a valores por defecto.
 */
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
    
    // Inicializar pila al final de la memoria (RX es base fija)
    registrosCpu.rx = TAMANO_MEMORIA - 1;  // Base fija de pila (1999)
    registrosCpu.sp = TAMANO_MEMORIA - 1;  // Tope de pila (cambia durante ejecucion)
    
    // Inicializar PSW
    registrosCpu.psw.codigoCondicion = CC_CERO;
    registrosCpu.psw.modoOperacion = MODO_USUARIO;
    registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
    registrosCpu.psw.pc = 0;  // PC inicia en 0 (logico)
    
    // Reiniciar contadores
    contadorCiclos = 0;
    intervaloReloj = 0;
    interrupcionPendiente = 0;
    codigoInterrupcionPendiente = -1;
    cpuEjecutando = 0;
    
    imprimirLog("CPU inicializado correctamente");
}

/*
 * Bucle principal del CPU. Ejecuta ciclos hasta centinela o error fatal.
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
        
        // Verificar interrupciones pendientes
        if (interrupcionPendiente && registrosCpu.psw.habilitarInterrupciones) {
            if (!manejarInterrupcion(codigoInterrupcionPendiente)) {
                // Interrupcion fatal, detener
                break;
            }
            interrupcionPendiente = 0;
            codigoInterrupcionPendiente = -1;
        }
        
        // Verificar interrupcion de reloj
        if (intervaloReloj > 0) {
            contadorCiclos++;
            if (contadorCiclos >= intervaloReloj) {
                contadorCiclos = 0;
                if (registrosCpu.psw.habilitarInterrupciones) {
                    manejarInterrupcion(INT_TIMER);
                }
            }
        }
        
        // Verificar interrupcion del DMA (E/S completada)
        if (verificarInterrupcionDma() && registrosCpu.psw.habilitarInterrupciones) {
            interrupcionPendienteDma = 0;  // Limpiar la bandera
            manejarInterrupcion(INT_IO_DONE);
        }
        
        /*
         * NOTA MULTIPROGRAMACION:
         * Aqui es donde el planificador verificaria si hay que hacer
         * un cambio de contexto. Se llamaria a:
         *   if (planificador_debe_cambiar()) {
         *       guardar_pcb(proceso_actual);
         *       proceso_actual = planificador_siguiente();
         *       cargar_pcb(proceso_actual);
         *   }
         */
    }
    
    // Mostrar estado final
    imprimirLog("CPU detenido");
    imprimirEstadoCpu();
}

/* ============================================================================
 * CICLO DE INSTRUCCION
 * ============================================================================ */

/*
 * Ejecuta un ciclo completo: FETCH, DECODE, EXECUTE.
 * Retorna 1 para continuar, 0 para detener.
 */
int cicloCpu() {
    char buffer[100];
    
    // 1. FETCH
    faseFetch();
    
    // Verificar si es centinela
    if (esCentinela(registrosCpu.mdr)) {
        imprimirLog("Centinela detectado - Fin del programa");
        cpuEjecutando = 0;
        return 0;
    }
    
    // 2. DECODE
    faseDecode();
    
    sprintf(buffer, "FETCH-DECODE: Op=%02d Dir=%d Val=%05d en PC=%05d",
            registrosCpu.ir.codigoOperacion,
            registrosCpu.ir.direccionamiento,
            registrosCpu.ir.valor,
            registrosCpu.psw.pc - 1);
    imprimirLog(buffer);
    
    // 3. EXECUTE
    return faseExecute();
}

/*
 * Fase FETCH: Busca la instruccion de memoria.
 */
void faseFetch() {
    int direccionFisica;
    
    // MAR <- PC (direccion logica)
    registrosCpu.mar = enteroAPalabra(registrosCpu.psw.pc);
    
    // Traducir a direccion fisica
    direccionFisica = traducirDireccion(registrosCpu.psw.pc);
    
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

/*
 * Fase DECODE: Decodifica la instruccion en IR.
 */
void faseDecode() {
    // IR <- MDR (la instruccion esta en MDR)
    // Formato: [Signo][OODDVVVVV] donde OO=opcode, D=modo, VVVVV=valor
    int instruccionCompleta = registrosCpu.mdr.digitos;
    
    // Extraer campos de la instruccion
    // Los 7 digitos: OODVVVVV
    registrosCpu.ir.codigoOperacion = instruccionCompleta / 100000;        // Primeros 2
    registrosCpu.ir.direccionamiento = (instruccionCompleta / 10000) % 10; // 3er digito
    registrosCpu.ir.valor = instruccionCompleta % 10000;                   // Ultimos 5
    
    // Ajustar si el valor tiene 5 digitos (puede ser hasta 99999)
    registrosCpu.ir.valor = instruccionCompleta % 100000;
    registrosCpu.ir.direccionamiento = (instruccionCompleta / 100000) % 10;
    registrosCpu.ir.codigoOperacion = instruccionCompleta / 1000000;
}

/*
 * Fase EXECUTE: Ejecuta la instruccion decodificada.
 * Retorna 1 para continuar, 0 para detener.
 */
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
        /* ===== GRUPO 1: ARITMETICAS ===== */
        case OP_SUM:  // 00: AC = AC + dato
            operando = obtenerOperando(modo, valor);
            valorAc = palabraAEntero(registrosCpu.ac);
            valorOp = palabraAEntero(operando);
            resultado = valorAc + valorOp;
            registrosCpu.ac = enteroAPalabra(resultado);
            actualizarCodigoCondicion(resultado);
            break;
            
        case OP_RES:  // 01: AC = AC - dato
            operando = obtenerOperando(modo, valor);
            valorAc = palabraAEntero(registrosCpu.ac);
            valorOp = palabraAEntero(operando);
            resultado = valorAc - valorOp;
            registrosCpu.ac = enteroAPalabra(resultado);
            actualizarCodigoCondicion(resultado);
            break;
            
        case OP_MULT:  // 02: AC = AC * dato
            operando = obtenerOperando(modo, valor);
            valorAc = palabraAEntero(registrosCpu.ac);
            valorOp = palabraAEntero(operando);
            resultado = valorAc * valorOp;
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
            imprimirLog("Advertencia: RX es base fija de la pila y no puede modificarse");
            // RX se mantiene fijo en TAMANO_MEMORIA - 1 según especificación
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
            
        case OP_JMPE:  // 09: Salta si AC == M[SP]
            valorAc = palabraAEntero(registrosCpu.ac);
            valorOp = palabraAEntero(leerMemoria(registrosCpu.sp));
            if (valorAc == valorOp) {
                // Verificar que la direccion destino este dentro de limites
                if (!verificarDireccionSalto(valor)) {
                    imprimirLog("ERROR: Salto fuera de limites RB/RL");
                    interrupcionPendiente = 1;
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.psw.pc = valor;
            }
            break;
            
        case OP_JMPNE:  // 10: Salta si AC != M[SP]
            valorAc = palabraAEntero(registrosCpu.ac);
            valorOp = palabraAEntero(leerMemoria(registrosCpu.sp));
            if (valorAc != valorOp) {
                // Verificar que la direccion destino este dentro de limites
                if (!verificarDireccionSalto(valor)) {
                    imprimirLog("ERROR: Salto fuera de limites RB/RL");
                    interrupcionPendiente = 1;
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.psw.pc = valor;
            }
            break;
            
        case OP_JMPLT:  // 11: Salta si AC < M[SP]
            valorAc = palabraAEntero(registrosCpu.ac);
            valorOp = palabraAEntero(leerMemoria(registrosCpu.sp));
            if (valorAc < valorOp) {
                // Verificar que la direccion destino este dentro de limites
                if (!verificarDireccionSalto(valor)) {
                    imprimirLog("ERROR: Salto fuera de limites RB/RL");
                    interrupcionPendiente = 1;
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.psw.pc = valor;
            }
            break;
            
        case OP_JMPLGT:  // 12: Salta si AC > M[SP]
            valorAc = palabraAEntero(registrosCpu.ac);
            valorOp = palabraAEntero(leerMemoria(registrosCpu.sp));
            if (valorAc > valorOp) {
                // Verificar que la direccion destino este dentro de limites
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
            registrosCpu.ac = enteroAPalabra(registrosCpu.rb);
            break;
            
        case OP_STRRB:  // 20: RB = AC
            {
                int nuevoRB = palabraAEntero(registrosCpu.ac);
                // RB debe estar dentro del espacio de usuario y menor que RL
                if (nuevoRB < INICIO_MEMORIA_USUARIO || nuevoRB >= registrosCpu.rl) {
                    imprimirLog("ERROR: Valor invalido para RB");
                    interrupcionPendiente = 1;
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.rb = nuevoRB;
            }
            break;
            
        case OP_LOADRL:  // 21: AC = RL
            registrosCpu.ac = enteroAPalabra(registrosCpu.rl);
            break;
            
        case OP_STRRL:  // 22: RL = AC
            {
                int nuevoRL = palabraAEntero(registrosCpu.ac);
                // RL >= RB y dentro de memoria
                if (nuevoRL < registrosCpu.rb || nuevoRL >= TAMANO_MEMORIA) {
                    imprimirLog("ERROR: Valor invalido para RL");
                    interrupcionPendiente = 1;
                    codigoInterrupcionPendiente = INT_DIRECCION_INVALIDA;
                    return 1;
                }
                registrosCpu.rl = nuevoRL;
            }
            break;
            
        case OP_LOADSP:  // 23: AC = SP
            registrosCpu.ac = enteroAPalabra(registrosCpu.sp);
            break;
            
        case OP_STRSP:  // 24: SP = AC
            {
                int nuevoSP = palabraAEntero(registrosCpu.ac);
                // SP debe estar entre RB y RX (inclusive RX es base de pila)
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

/* ============================================================================
 * MANEJO DE INTERRUPCIONES
 * ============================================================================ */

/*
 * Guarda el contexto actual del CPU.
 */
void guardarContexto(ContextoCpu *contexto) {
    contexto->ac = registrosCpu.ac;
    contexto->rb = registrosCpu.rb;
    contexto->rl = registrosCpu.rl;
    contexto->sp = registrosCpu.sp;
    contexto->psw = registrosCpu.psw;
    
    /*
     * NOTA MULTIPROGRAMACION:
     * Aqui se guardaria el contexto en el PCB del proceso actual:
     *   proceso_actual->pcb.contexto = *contexto;
     */
}

/*
 * Restaura el contexto del CPU.
 */
void restaurarContexto(ContextoCpu *contexto) {
    registrosCpu.ac = contexto->ac;
    registrosCpu.rb = contexto->rb;
    registrosCpu.rl = contexto->rl;
    /* RX es la base fija de la pila y debe permanecer en TAMANO_MEMORIA-1 */
    registrosCpu.rx = TAMANO_MEMORIA - 1;
    registrosCpu.sp = contexto->sp;
    registrosCpu.psw = contexto->psw;
    
    /*
     * NOTA MULTIPROGRAMACION:
     * Aqui se cargaria el contexto desde el PCB del nuevo proceso:
     *   *contexto = nuevo_proceso->pcb.contexto;
     */
}

/* ============================================================================
 * Helpers para guardar/restaurar en pila (push/pop internos)
 * ============================================================================ */

// Empuja una Palabra en la pila. Retorna 1 ok, 0 si overflow.
static int pushPalabra(Palabra p) {
    registrosCpu.sp--;
    if (registrosCpu.sp < registrosCpu.rb) {
        registrosCpu.sp++; // revertir
        return 0;
    }
    escribirMemoria(registrosCpu.sp, p);
    return 1;
}

// Empuja un entero convirtiendolo en Palabra
static int pushInt(int v) {
    return pushPalabra(enteroAPalabra(v));
}

// Saca una Palabra de la pila (pop)
static Palabra popPalabra() {
    Palabra p = leerMemoria(registrosCpu.sp);
    registrosCpu.sp++;
    return p;
}

// Saca un entero de la pila
static int popInt() {
    return palabraAEntero(popPalabra());
}


/*
 * Maneja una interrupcion. Retorna 1 si es recuperable, 0 si es fatal.
 */
int manejarInterrupcion(int codigoInterrupcion) {
    char buffer[100];
    int esRecuperable = 0;
    
    sprintf(buffer, "=== INTERRUPCION %d ===", codigoInterrupcion);
    imprimirLog(buffer);
    printf("[INTERRUPCION] Codigo: %d\n", codigoInterrupcion);

    // 1. Determinar si es recuperable (no modificar estado todavía)
    switch (codigoInterrupcion) {
        case INT_SYSCALL_INVALIDA:  // 0: Syscall invalida - FATAL
            imprimirLog("ERROR FATAL: Syscall invalida");
            esRecuperable = 0;
            break;

        case INT_CODIGO_INVALIDO:  // 1: Codigo invalido - FATAL
            imprimirLog("ERROR FATAL: Codigo de interrupcion invalido");
            esRecuperable = 0;
            break;

        case INT_SVC:  // 2: Llamada al sistema - RECUPERABLE
            imprimirLog("Manejando syscall...");
            esRecuperable = 1;
            break;

        case INT_TIMER:  // 3: Timer - RECUPERABLE
            imprimirLog("Interrupcion de reloj");
            esRecuperable = 1;
            break;

        case INT_IO_DONE:  // 4: Fin de E/S - RECUPERABLE
            imprimirLog("Operacion de E/S completada");
            esRecuperable = 1;
            break;

        case INT_INSTRUCCION_INVALIDA:  // 5: Instruccion invalida - FATAL
            imprimirLog("ERROR FATAL: Instruccion invalida o privilegiada");
            esRecuperable = 0;
            break;

        case INT_DIRECCION_INVALIDA:  // 6: Direccionamiento invalido - FATAL
            imprimirLog("ERROR FATAL: Violacion de proteccion de memoria");
            esRecuperable = 0;
            break;

        case INT_UNDERFLOW:  // 7: Underflow - FATAL
            imprimirLog("ERROR FATAL: Stack underflow");
            esRecuperable = 0;
            break;

        case INT_OVERFLOW:  // 8: Overflow - FATAL
            imprimirLog("ERROR FATAL: Overflow aritmetico");
            esRecuperable = 0;
            break;

        default:
            sprintf(buffer, "ERROR: Codigo de interrupcion desconocido: %d", codigoInterrupcion);
            imprimirLog(buffer);
            esRecuperable = 0;
    }

    // 2. Si es fatal, terminar programa
    if (!esRecuperable) {
        imprimirLog("Interrupcion fatal - Terminando programa");
        cpuEjecutando = 0;
        return 0;
    }

    // 3. Es recuperable: intentar guardar todo el contexto en la pila
    //    Orden de push (de primero a ultimo): AC, MAR, MDR,
    //    IR.codigo, IR.direccionamiento, IR.valor,
    //    RB, RL, RX,
    //    PSW.codigoCondicion, PSW.modoOperacion, PSW.habilitarInterrupciones, PSW.pc

    // Guardar AC, MAR, MDR
    if (!pushPalabra(registrosCpu.ac)) {
        imprimirLog("ERROR: Stack overflow al guardar contexto (AC)");
        cpuEjecutando = 0;
        return 0;
    }
    if (!pushPalabra(registrosCpu.mar)) {
        imprimirLog("ERROR: Stack overflow al guardar contexto (MAR)");
        cpuEjecutando = 0;
        return 0;
    }
    if (!pushPalabra(registrosCpu.mdr)) {
        imprimirLog("ERROR: Stack overflow al guardar contexto (MDR)");
        cpuEjecutando = 0;
        return 0;
    }

    // IR fields
    if (!pushInt(registrosCpu.ir.codigoOperacion)) { imprimirLog("ERROR: Stack overflow (IR.cod)"); cpuEjecutando = 0; return 0; }
    if (!pushInt(registrosCpu.ir.direccionamiento)) { imprimirLog("ERROR: Stack overflow (IR.dir)"); cpuEjecutando = 0; return 0; }
    if (!pushInt(registrosCpu.ir.valor)) { imprimirLog("ERROR: Stack overflow (IR.val)"); cpuEjecutando = 0; return 0; }

    // RB, RL, RX
    if (!pushInt(registrosCpu.rb)) { imprimirLog("ERROR: Stack overflow (RB)"); cpuEjecutando = 0; return 0; }
    if (!pushInt(registrosCpu.rl)) { imprimirLog("ERROR: Stack overflow (RL)"); cpuEjecutando = 0; return 0; }
    /* RX es base fija de la pila y no se debe modificar/restaurar desde el contexto
     * No empujamos RX en la pila para preservarlo como TAMANO_MEMORIA-1 */

    // PSW fields
    if (!pushInt(registrosCpu.psw.codigoCondicion)) { imprimirLog("ERROR: Stack overflow (PSW.CC)"); cpuEjecutando = 0; return 0; }
    if (!pushInt(registrosCpu.psw.modoOperacion)) { imprimirLog("ERROR: Stack overflow (PSW.MODO)"); cpuEjecutando = 0; return 0; }
    if (!pushInt(registrosCpu.psw.habilitarInterrupciones)) { imprimirLog("ERROR: Stack overflow (PSW.INT)"); cpuEjecutando = 0; return 0; }
    if (!pushInt(registrosCpu.psw.pc)) { imprimirLog("ERROR: Stack overflow (PSW.PC)"); cpuEjecutando = 0; return 0; }

    // 4. Cambiar a modo kernel y deshabilitar interrupciones mientras se maneja
    registrosCpu.psw.modoOperacion = MODO_KERNEL;
    registrosCpu.psw.habilitarInterrupciones = INT_DESHABILITADAS;

    // 5. Ejecutar manejador (simplificado / placeholders)
    switch (codigoInterrupcion) {
        case INT_SVC:
            // handler de syscall (placeholder)
            break;
        case INT_TIMER:
            // handler de timer (placeholder)
            break;
        case INT_IO_DONE:
            // handler E/S completada (placeholder)
            break;
        default:
            // No hay accion adicional para los otros casos recuperables
            break;
    }

    // 6. Restaurar contexto desde pila (orden inverso al push)
    registrosCpu.psw.pc = popInt();
    registrosCpu.psw.habilitarInterrupciones = popInt();
    registrosCpu.psw.modoOperacion = popInt();
    registrosCpu.psw.codigoCondicion = popInt();

    /* RX no fue apilado: asegurar que permanezca en la base fija */
    registrosCpu.rl = popInt();
    registrosCpu.rb = popInt();
    registrosCpu.rx = TAMANO_MEMORIA - 1;

    registrosCpu.ir.valor = popInt();
    registrosCpu.ir.direccionamiento = popInt();
    registrosCpu.ir.codigoOperacion = popInt();

    registrosCpu.mdr = popPalabra();
    registrosCpu.mar = popPalabra();
    registrosCpu.ac = popPalabra();

    // 7. Devolver a modo usuario (PSW ya restaurado, pero asegurar bandera de interrupciones)
    registrosCpu.psw.modoOperacion = MODO_USUARIO;
    registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
    imprimirLog("Retornando de interrupcion");

    return 1;
}

/*
 * Verifica si hay interrupciones pendientes externas.
 * Retorna codigo de interrupcion o -1 si no hay.
 */
int verificarInterrupcionesPendientes() {
    /*
     * NOTA: Aqui se verificarian las lineas de interrupcion del hardware:
     * - DMA terminado
     * - Timer expirado
     * - Etc.
     * 
     * Por ahora solo se verifica la bandera global.
     */
    if (interrupcionPendiente) {
        return codigoInterrupcionPendiente;
    }
    return -1;
}
