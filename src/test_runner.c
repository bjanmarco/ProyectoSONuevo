#include <stdio.h>
#include "../include/hardware.h"
#include "../include/memoria.h"
#include "../include/disco.h"
#include "../include/dma.h"
#include "../include/loader.h"
#include "../include/logger.h"
#include "../include/cpu.h"

// Probar un programa dado (instrucciones como enteros)
int run_program(int *instrs, int n, int start_logico) {
    Palabra p;
    int base = INICIO_MEMORIA_USUARIO;

    inicializarMemoria();
    inicializarDisco();
    inicializarDma();
    inicializarLoader();
    inicializarCpu();

    // Escribir instrucciones en memoria
    for (int i = 0; i < n; i++) {
        p.signo = 0;
        p.digitos = instrs[i];
        escribirMemoria(base + i, p);
        printf("[TEST] Wrote instr %d at %d -> %08d\n", i, base + i, instrs[i]);
    }

    // Configurar registros para ejecutar
    registrosCpu.rb = base;
    registrosCpu.rl = base + n - 1;
    registrosCpu.psw.pc = start_logico;
    registrosCpu.rx = 1999;
    registrosCpu.sp = 1999;

    printf("[TEST] Running program: RB=%d RL=%d PC=%d RX=%d SP=%d\n",
           registrosCpu.rb, registrosCpu.rl, registrosCpu.psw.pc,
           registrosCpu.rx, registrosCpu.sp);

    ejecutarCpu();

    imprimirEstadoCpu();
    return 0;
}

int main() {
    // Test SVC: push, push, svc, centinela
    int test_svc[] = {25000000, 25000000, 13000000, 99000000};
    run_program(test_svc, 4, 0);

    // Test fatal: invalid opcode 88
    int test_fatal[] = {88000000, 99000000};
    run_program(test_fatal, 2, 0);

    return 0;
}
