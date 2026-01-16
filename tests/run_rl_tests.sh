#!/bin/bash
set -e
ROOT=$(dirname "$0")/..
BIN="$ROOT/bin/maquina_virtual"

run_test() {
  prog=$1
  echo "--- Ejecutando test: $prog ---"
  printf "cargar bin/$prog\nrun\nsalir\n" | $BIN |& grep -E "Fin del programa: PC > RL|ERROR: PC < RB" || true
}

run_test prog_end_by_rl.prog
run_test prog_two_instructions.prog

echo "Tests finalizados." 
