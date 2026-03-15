// Carga los archivos de programa en la memoria RAM
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

// Asignacion dinamica de memoria

// Indice global de la posicion en disco para grabar el siguiente archivo
int siguienteCilindroDisponible = 0;
int siguientePistaDisponible = 0;
int siguienteSectorDisponible = 0;

// Arreglo FAT
DirectorioPrograma directorioDisco[MAX_PROGRAMAS_DISCO];

// Informacion del programa actual
InfoPrograma programaActual;

// Referencia externa a los registros del CPU
extern Registros registrosCpu;

// Funcion externa para reiniciar deteccion de bucles
extern void reiniciarDeteccionBucle();

static void limpiarProgramaActual() {
    memset(programaActual.nombre, 0, MAX_NOMBRE_PROGRAMA);
    programaActual.lineaInicio = 0;
    programaActual.numeroPalabras = 0;
    programaActual.direccionBase = 0;
    programaActual.direccionLimite = 0;
}


// Busca un hueco continuo en la RAM que no colisione con procesos activos
static int buscarEspacioLibreMemoria(int tamañoNecesario) {
    int inicioBusqueda = INICIO_MEMORIA_USUARIO;
    int limiteMaximo = TAMANO_MEMORIA - 1;

    while (inicioBusqueda <= limiteMaximo - tamañoNecesario) {
        int solapamiento = 0;
        int proximoSalto = inicioBusqueda + 1;

        for (int i = 0; i < MAX_PROCESOS; i++) {
            // Solo considerar procesos VIVOS (NUEVO, LISTO, EJECUCION, DORMIDO)
            if (tablaProcesos[i].id != -1 && tablaProcesos[i].estado != ESTADO_TERMINADO) {
                int pBase = tablaProcesos[i].direccionBase;
                int pLim = tablaProcesos[i].direccionLimite;
                
                // Verificar si hay interseccion de intervalos
                if (inicioBusqueda <= pLim && (inicioBusqueda + tamañoNecesario - 1) >= pBase) {
                    solapamiento = 1;
                    if (pLim + 1 > proximoSalto) {
                        proximoSalto = pLim + 1; // Saltar todo el bloque ocupado
                    }
                }
            }
        }
        
        if (!solapamiento) {
            return inicioBusqueda;
        }
        inicioBusqueda = proximoSalto;
    }
    return -1; // No hay espacio continuo
}

void inicializarLoader() {
    siguienteCilindroDisponible = 0;
    siguientePistaDisponible = 0;
    siguienteSectorDisponible = 0;
    
    // Limpiar el arreglo FAT
    for (int i = 0; i < MAX_PROGRAMAS_DISCO; i++) {
        directorioDisco[i].ocupado = 0;
    }

    limpiarProgramaActual();
    logLoader("Loader inicializado con algoritmo First-Fit (Reaprovechamiento). Direccion base dinamica.");
}

int cargarProgramaEnDisco(const char *rutaArchivo) {
    FILE *archivo = NULL;
    char linea[MAX_LINEA];
    int lineaInicio = -1, numeroPalabrasHeader = -1;
    char nombrePrograma[MAX_NOMBRE_PROGRAMA] = "";
    int resultado = 1;

    // Buscar espacio libre en el Directorio FAT
    int indiceFAT = -1;
    for (int i = 0; i < MAX_PROGRAMAS_DISCO; i++) {
        if (!directorioDisco[i].ocupado) {
            indiceFAT = i;
            break;
        } else if (strcmp(directorioDisco[i].nombre, rutaArchivo) == 0) {
            logLoader("Programa '%s' ya existe en Disco Duro. Omitiendo carga.", rutaArchivo);
            return 0;
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

    // Abrir el archivo
    archivo = fopen(rutaArchivo, "r");
    if (archivo == NULL) {
        logLoader("ERROR: No se pudo abrir el archivo %s", rutaArchivo);
        return 1;
    }

    // Leer y validar el archivo
    while (fgets(linea, MAX_LINEA, archivo) != NULL) {
        linea[strcspn(linea, "\r\n")] = '\0';

        // Ignorar lineas vacias y comentarios
        if (strlen(linea) == 0 || linea[0] == '/' || linea[0] == '#' || linea[0] == '.' || strncmp(linea, "_start", 6) == 0) {
            // Parsear metadata especial
            if (strncmp(linea, "_start", 6) == 0) {
                sscanf(linea, "_start %d", &lineaInicio);
            } else if (strncmp(linea, ".NumeroPalabras", 15) == 0) {
                sscanf(linea, ".NumeroPalabras %d", &numeroPalabrasHeader);
            } else if (strncmp(linea, ".NombreProg", 11) == 0) {
                sscanf(linea, ".NombreProg %49s", nombrePrograma);
            } else if (linea[0] == '.' && strlen(linea) == 1) {
                break;
            }
            continue;
        }
        
        // Conversion Robusta
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


        // Agregar al buffer dinamico
        if (bufferLen >= bufferCap) {
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

    // Volcado a disco duro
    // Verificar capacidad
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

    // Escribir instrucciones al disco
    for (int i = 0; i < bufferLen; i++) {
        char tempStr[10];
        snprintf(tempStr, sizeof(tempStr), "%d%08d", buffer[i].signo, buffer[i].digitos);
        
        escribirSectorDisco(siguientePistaDisponible, siguienteCilindroDisponible, siguienteSectorDisponible, tempStr);
        
        // Secuencia de disco
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
    resultado = 0;

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


    int tamPart = progFAT.numeroPalabras + 20; 
    tamPart = (tamPart > 85) ? tamPart : 85;

    int direccionBase = buscarEspacioLibreMemoria(tamPart);
    if (direccionBase == -1) {
        logLoader("ERROR: Memoria RAM insuficiente y fragmentada para volcar %s desde Disco.", nombrePrograma);
        printf("[LOADER] ERROR: No hay espacio seguro libre para el programa. Esperando finalizaciones.\n");
        return 1;
    }

    int direccionFin = direccionBase + tamPart - 1;
    int contienePrivilegiada = 0;

    // Limpiar hueco en RAM
    Palabra p0 = {0, 0};
    for (int i = direccionBase; i <= direccionFin; i++) {
        escribirMemoria(i, p0);
    }

    int s_cilindro = progFAT.cilindroInicio;
    int s_pista = progFAT.pistaInicio;
    int s_sector = progFAT.sectorInicio;
    
    char sectorCrudo[TAMANO_SECTOR + 1];

    for (int i = 0; i < progFAT.numeroPalabras; i++) {
        leerSectorDisco(s_pista, s_cilindro, s_sector, sectorCrudo);
        sectorCrudo[TAMANO_SECTOR] = '\0';
        
        Palabra p;
        p.signo = sectorCrudo[0] - '0';
        p.digitos = atoi(&sectorCrudo[1]);
        
        // Extraccion de opcode
        char opcodeStr[3];
        opcodeStr[0] = sectorCrudo[1];
        opcodeStr[1] = sectorCrudo[2];
        opcodeStr[2] = '\0';
        
        int opcodeLeido = atoi(opcodeStr);
        if (esInstruccionPrivilegiada(opcodeLeido)) {
            contienePrivilegiada = 1;
        }
        
        escribirMemoria(direccionBase + i, p);

        // Avance de disco
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


    int nuevoPID = crearProceso(progFAT.nombre, direccionBase, direccionFin, progFAT.lineaInicio - 1, progFAT.numeroPalabras, contienePrivilegiada);
    
    if (nuevoPID != -1) {
        logLoader("Programa '%s' volcado de DISCO a RAM [%d-%d] con PID %d", progFAT.nombre, direccionBase, direccionFin, nuevoPID);
        return 0; 
    } else {
        logLoader("ERROR RAM: Fallo creacion BCP (Supero limite de procesos vivos).");
        return 1; 
    }
}

void prepararEjecucion() {
    // Reiniciar deteccion de bucles
    reiniciarDeteccionBucle();

    // Establecer registros de proteccion
    registrosCpu.rb = programaActual.direccionBase;
    registrosCpu.rl = programaActual.direccionLimite;

    // Establecer PC en la linea de inicio logica
    registrosCpu.psw.pc = programaActual.lineaInicio;

    // Establecer pila al FINAL de la memoria
    registrosCpu.rx = TAMANO_MEMORIA - 1;
    registrosCpu.sp = TAMANO_MEMORIA - 1;

    // Modo usuario con interrupciones habilitadas
    registrosCpu.psw.modoOperacion = MODO_USUARIO;
    registrosCpu.psw.habilitarInterrupciones = INT_HABILITADAS;
    registrosCpu.psw.codigoCondicion = CC_CERO;

    // Limpiar acumulador
    registrosCpu.ac.signo = 0;
    registrosCpu.ac.digitos = 0;

    // Limpiar interrupciones
    interrupcionPendiente = 0;
    for(int i=0; i<NUM_INTERRUPCIONES; i++) interrupcionesPendientes[i] = 0;

    logLoader("CPU preparado para ejecucion:");
    logLoader("  RB=%d, RL=%d, PC=%d (logico)",
           registrosCpu.rb, registrosCpu.rl, registrosCpu.psw.pc);
    logLoader("  RX=%d, SP=%d",
           registrosCpu.rx, registrosCpu.sp);
}
