// este modulo lee los archivos de programa 
// y los carga en la memoria RAM para que el CPU los pueda ejecutar.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/loader.h"
#include "../include/memoria.h"
#include "../include/hardware.h"
#include "../include/logger.h"
#include "../include/cpu.h"

#include "../include/disco.h"
#include "../include/procesos.h"

// siguiente direccion de memoria disponible para cargar programas
// inicia en 300 
int siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO;

// Indice global de la posicion en disco para grabar el siguiente archivo
int siguienteCilindroDisponible = 0;
int siguientePistaDisponible = 0;
int siguienteSectorDisponible = 0;

// Arreglo FAT
DirectorioPrograma directorioDisco[MAX_PROGRAMAS_DISCO];

// esta es el struct de la info el programa.
InfoPrograma programaActual;

// referencia externa a los registros del CPU (definidos en cpu.c)
extern Registros registrosCpu;

// funcion externa para reiniciar deteccion de bucles (definida en cpu.c)
extern void reiniciarDeteccionBucle();

static void limpiarProgramaActual() {
    // memeset es para llenar la memoria con 0 y asi borras lo que tenia antes
    memset(programaActual.nombre, 0, MAX_NOMBRE_PROGRAMA);
    programaActual.lineaInicio = 0;
    programaActual.numeroPalabras = 0;
    programaActual.direccionBase = 0;
    programaActual.direccionLimite = 0;
    //y el struct queda listo para guarda la nueva info del siguiente programa
}

void inicializarLoader() {
    siguienteDireccionDisponible = INICIO_MEMORIA_USUARIO; // partimos de la base del usuario
    siguienteCilindroDisponible = 0;
    siguientePistaDisponible = 0;
    siguienteSectorDisponible = 0;
    
    // Limpiar el arreglo FAT
    for (int i = 0; i < MAX_PROGRAMAS_DISCO; i++) {
        directorioDisco[i].ocupado = 0;
    }

    limpiarProgramaActual();
    logLoader("Loader inicializado. Direccion base RAM: %d", 
    // usamos el Macro El atajo para imprimir el valor de la siguiente direccion disponible
           siguienteDireccionDisponible);
}

int cargarProgramaEnDisco(const char *rutaArchivo) {
    FILE *archivo = NULL;
    char linea[MAX_LINEA];
    int lineaInicio = -1, numeroPalabrasHeader = -1;
    char nombrePrograma[MAX_NOMBRE_PROGRAMA] = "";
    int resultado = 1;  // por defecto error, cambia a 0 si todo sale bien

    // Buscar espacio libre en el Directorio FAT
    int indiceFAT = -1;
    for (int i = 0; i < MAX_PROGRAMAS_DISCO; i++) {
        if (!directorioDisco[i].ocupado) {
            indiceFAT = i;
            break;
        } else if (strcmp(directorioDisco[i].nombre, rutaArchivo) == 0) {
            // Ya está listado en el FAT
            logLoader("Programa '%s' ya existe en Disco Duro. Omitiendo carga.", rutaArchivo);
            return 0; // Exito automatico
        }
    }

    if (indiceFAT == -1) {
        logLoader("ERROR: DiscoFAT lleno, maximo %d programas.", MAX_PROGRAMAS_DISCO);
        return 1;
    }

    // buffer temporal para validar antes de escribir
    Palabra *buffer = NULL;
    int bufferCap = 0, bufferLen = 0;

    logLoader("Intentando cargar: %s", rutaArchivo);

    // abrir el archivo
    archivo = fopen(rutaArchivo, "r"); // abrimos leyendo
    if (archivo == NULL) {
        logLoader("ERROR: No se pudo abrir el archivo %s", rutaArchivo);
        return 1;
    }

    // leer y validar el archivo
    while (fgets(linea, MAX_LINEA, archivo) != NULL) {
        linea[strcspn(linea, "\n")] = '\0';  // eliminar salto de linea

        // ignorar lineas vacias y comentarios explicitos al inicio
        if (strlen(linea) == 0 || linea[0] == '/' || linea[0] == '#' || linea[0] == '.' || strncmp(linea, "_start", 6) == 0) {
            // parsear metadata especial antes de continuar
            if (strncmp(linea, "_start", 6) == 0) {
                sscanf(linea, "_start %d", &lineaInicio);
            } else if (strncmp(linea, ".NumeroPalabras", 15) == 0) {
                sscanf(linea, ".NumeroPalabras %d", &numeroPalabrasHeader);
            } else if (strncmp(linea, ".NombreProg", 11) == 0) {
                sscanf(linea, ".NombreProg %49s", nombrePrograma);
            } else if (linea[0] == '.' && strlen(linea) == 1) {
                break; // fin de programa
            }
            continue;
        }
        
        // Conversión Robusta (Estilo strtoll ignorará sufijos y comentarios tabulados)
        long valorLargo = strtol(linea, NULL, 10);
        
        // Determinar signo manual o extraido del strtol
        int tieneSigno = 0;
        if (linea[0] == '1' && strlen(linea) >= 9) {
            tieneSigno = 1;
            valorLargo = strtol(&linea[1], NULL, 10); 
        } else if (linea[0] == '0' && strlen(linea) >= 9) {
            tieneSigno = 0;
            valorLargo = strtol(&linea[1], NULL, 10);
        } else if (valorLargo < 0) {
            tieneSigno = 1;
            valorLargo = -valorLargo;
        }

        // Validaciones estrictas
        if (valorLargo < 0 || valorLargo > 99999999L) {
            logLoader("ERROR: Valor de instruccion fuera de rango o con error de parseo: %ld", valorLargo);
            goto cleanup; 
        }

        Palabra instruccion;
        instruccion.signo = tieneSigno;
        instruccion.digitos = (int)valorLargo;


        // agregar al buffer dinamico (expandir si es necesario)
        if (bufferLen >= bufferCap) { // el len son las que llevamos y el cap las totales
            int nuevaCap = (bufferCap == 0) ? 16 : bufferCap * 2;
            Palabra *tmp = (Palabra*)realloc(buffer, nuevaCap * sizeof(Palabra));
            if (tmp == NULL) {
                logLoader("ERROR: No hay memoria para buffer");
                goto cleanup; 
            }
            buffer = tmp; bufferCap = nuevaCap; 
        }
        buffer[bufferLen++] = instruccion; 
    }

    fclose(archivo);
    archivo = NULL; 

    if (bufferLen == 0) {
        logLoader("ERROR: No se encontraron instrucciones");
        goto cleanup;
    }

    if (numeroPalabrasHeader != -1 && numeroPalabrasHeader != bufferLen) {
        logLoader("ERROR: .NumeroPalabras (%d) no coincide con instrucciones leidas (%d)",
               numeroPalabrasHeader, bufferLen);
        goto cleanup;
    }

    if (lineaInicio < 1 || lineaInicio > bufferLen) {
        logLoader("ERROR: _start invalido o fuera de rango (debe ser 1..%d)", bufferLen);
        goto cleanup;
    }

    // --- NUEVO: VOLCADO A DISCO DURO (VERIFICANDO CAPACIDAD) ---
    // Chequear si caben las instrucciones
    int requeridosSectores = bufferLen;
    int disponible = (DISCO_CILINDROS * DISCO_PISTAS * DISCO_SECTORES) - 
                     ((siguienteCilindroDisponible * DISCO_PISTAS * DISCO_SECTORES) + 
                      (siguientePistaDisponible * DISCO_SECTORES) + 
                      siguienteSectorDisponible);
                      
    if (requeridosSectores > disponible) {
        logLoader("ERROR: Disdo Duro Lleno.");
        goto cleanup;
    }

    // Escribir en la FAT
    strcpy(directorioDisco[indiceFAT].nombre, rutaArchivo);
    directorioDisco[indiceFAT].lineaInicio = lineaInicio;
    directorioDisco[indiceFAT].numeroPalabras = bufferLen;
    directorioDisco[indiceFAT].cilindroInicio = siguienteCilindroDisponible;
    directorioDisco[indiceFAT].pistaInicio = siguientePistaDisponible;
    directorioDisco[indiceFAT].sectorInicio = siguienteSectorDisponible;
    directorioDisco[indiceFAT].ocupado = 1;

    // Escribir cada instrucción al disco simulado
    for (int i = 0; i < bufferLen; i++) {
        char tempStr[10];
        snprintf(tempStr, sizeof(tempStr), "%d%08d", buffer[i].signo, buffer[i].digitos);
        
        escribirSectorDisco(siguientePistaDisponible, siguienteCilindroDisponible, siguienteSectorDisponible, tempStr);
        
        // Aritmetica de cabezales del disco
        siguienteSectorDisponible++;
        if (siguienteSectorDisponible >= DISCO_SECTORES) {
            siguienteSectorDisponible = 0;
            siguientePistaDisponible++;
            if (siguientePistaDisponible >= DISCO_PISTAS) {
                siguientePistaDisponible = 0;
                siguienteCilindroDisponible++;
            }
        }
    }

    logLoader("Carga en Disco completa. (FAT_ID: %d)", indiceFAT);
    resultado = 0; // Exito


// usamos la salida correcta 
cleanup:
    if (archivo != NULL) fclose(archivo);
    free(buffer);
    return resultado;
}

int cargarProgramaEnMemoria(const char *nombrePrograma) {
    int logicoID = -1;
    for (int i = 0; i < MAX_PROGRAMAS_DISCO; i++) {
        if (directorioDisco[i].ocupado && strcmp(directorioDisco[i].nombre, nombrePrograma) == 0) {
            logicoID = i;
            break;
        }
    }

    if (logicoID == -1) {
        logLoader("ERROR: %s no se encuentra en el disco duro.", nombrePrograma);
        printf("[LOADER] ERROR: Archivo no se encuentra en Disco: %s\n", nombrePrograma);
        return 1;
    }

    DirectorioPrograma progFAT = directorioDisco[logicoID];

    int direccionBase = siguienteDireccionDisponible;
    int direccionFin = direccionBase + progFAT.numeroPalabras;

    // Verificar memoria
    if (direccionFin >= TAMANO_MEMORIA - 50) {
        logLoader("ERROR: Memoria RAM insuficiente para volcar %s desde Disco.", nombrePrograma);
        printf("[LOADER] ERROR: No hay espacio seguro. Pilas en riesgo.\n");
        return 1;
    }

    // Volcado de Disco Duro a RAM
    int s_cilindro = progFAT.cilindroInicio;
    int s_pista = progFAT.pistaInicio;
    int s_sector = progFAT.sectorInicio;
    
    char sectorCrudo[TAMANO_SECTOR + 1]; // +1 para el nulo terminador 

    for (int i = 0; i < progFAT.numeroPalabras; i++) {
        leerSectorDisco(s_pista, s_cilindro, s_sector, sectorCrudo);
        sectorCrudo[TAMANO_SECTOR] = '\0';
        
        Palabra p;
        p.signo = sectorCrudo[0] - '0';
        p.digitos = atoi(&sectorCrudo[1]);
        
        escribirMemoria(direccionBase + i, p);

        // Aritmetica manual de avance
        s_sector++;
        if (s_sector >= DISCO_SECTORES) {
            s_sector = 0;
            s_pista++;
            if (s_pista >= DISCO_PISTAS) {
                s_pista = 0;
                s_cilindro++;
            }
        }
    }

    // Instancia del proceso BCP
    int tamPart = progFAT.numeroPalabras + 20; 
    tamPart = (tamPart > 85) ? tamPart : 85;

    int nuevoPID = crearProceso(progFAT.nombre, direccionBase, direccionBase + tamPart - 1, progFAT.lineaInicio - 1);
    
    if (nuevoPID != -1) {
        logLoader("Programa '%s' pasado de DISCO a RAM con PID %d", progFAT.nombre, nuevoPID);
        siguienteDireccionDisponible = direccionBase + tamPart;
        return 0; 
    } else {
        logLoader("ERROR RAM: Fallo creacion BCP.");
        return 1; 
    }
}

void prepararEjecucion() {
    // Reiniciar deteccion de bucles infinitos para evitar falsos positivos
    reiniciarDeteccionBucle();

    // Establecer registros de proteccion
    registrosCpu.rb = programaActual.direccionBase;
    registrosCpu.rl = TAMANO_MEMORIA - 50; // RL = limite maximo de usuario (protegiendo ultimos 50 de Pila)

    // Establecer PC en la linea de inicio (direccion logica)
    // Ya se convirtio a base 0 en cargarPrograma
    registrosCpu.psw.pc = programaActual.lineaInicio;

    // Establecer pila al FINAL de la memoria
    // La pila crece hacia abajo (SP--), por lo que iniciamos en la ultima posicion
    registrosCpu.rx = TAMANO_MEMORIA - 1;
    registrosCpu.sp = TAMANO_MEMORIA - 1;

    // Modo usuario con interrupciones habilitadas
    registrosCpu.psw.modoOperacion = MODO_USUARIO;
    registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
    registrosCpu.psw.codigoCondicion = CC_CERO;

    // Limpiar acumulador
    registrosCpu.ac.signo = 0;
    registrosCpu.ac.digitos = 0;

    // limpieza de estado de interrupciones
    // es fundamental limpiar cualquier interrupcion pendiente de ejecuciones anteriores
    // (especialmente si terminaron por error fatal), de lo contrario se dispararian
    // en el primer ciclo del nuevo programa.
    interrupcionPendiente = 0;
    for(int i=0; i<NUM_INTERRUPCIONES; i++) interrupcionesPendientes[i] = 0;

    logLoader("CPU preparado para ejecucion:");
    logLoader("  RB=%d, RL=%d, PC=%d (logico)",
           registrosCpu.rb, registrosCpu.rl, registrosCpu.psw.pc);
    logLoader("  RX=%d, SP=%d",
           registrosCpu.rx, registrosCpu.sp);
}
