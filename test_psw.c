#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int signo;
    int digitos;
} Palabra;

Palabra enteroAPalabra(int valor) {
    Palabra p;
    if (valor < 0) {
        p.signo = 1;
        p.digitos = -valor;
    } else {
        p.signo = 0;
        p.digitos = valor;
    }
    if (p.digitos > 99999999) {
        p.digitos = 99999999;
    }
    return p;
}

int palabraAEntero(Palabra p) {
    int valor = p.digitos;
    if (p.signo) {
        valor = -valor;
    }
    return valor;
}

int main() {
    int codigoCondicion = 0; // CC_CERO (0), CC_NEGATIVO (1), CC_POSITIVO (2), CC_DESBORDAMIENTO (3)
    int modoOperacion = 0;
    int habilitarInterrupciones = 1;
    int pc = 2;

    int valor = codigoCondicion * 10000000 + 
                modoOperacion * 1000000 + 
                habilitarInterrupciones * 100000 + 
                pc; 
    
    printf("Valor calculado: %d\n", valor);
    Palabra p = enteroAPalabra(valor);
    printf("Palabra en memoria: signo=%d, digitos=%d\n", p.signo, p.digitos);
    
    int recuperado = palabraAEntero(p);
    if (recuperado < 0) recuperado = -recuperado;
    
    int pc_final = recuperado % 100000;
    printf("PC Recperado: %d\n", pc_final);
    
    // Y si hubiere desbordamiento?
    int cc = 9; // si habia basura
    int val_malo = cc * 10000000 + modoOperacion * 1000000 + habilitarInterrupciones * 100000 + pc;
    printf("Valor basura: %d\n", val_malo);
    Palabra pmalo = enteroAPalabra(val_malo);
    printf("Palabra mala en memoria: signo=%d, digitos=%d\n", pmalo.signo, pmalo.digitos);
    int rec_malo = palabraAEntero(pmalo);
    int pc_malo = rec_malo % 100000;
    printf("PC Malo Recuperado: %d\n", pc_malo);
    
    return 0;
}
