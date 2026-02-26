#!/bin/bash

# Script para compilar y ejecutar todos los algoritmos de partición dinámica

echo "=========================================="
echo "COMPILANDO Y PROBANDO ALGORITMOS DE MEMORIA"
echo "=========================================="
echo ""

# Best Fit
echo "--- BEST FIT ---"
gcc -o clase1/part_dinamico_best_fit clase1/part_dinamico_best_fit.c && ./clase1/part_dinamico_best_fit
echo ""

# First Fit
echo "--- FIRST FIT ---"
gcc -o clase1/part_dinamico_first_fit clase1/part_dinamico_first_fit.c && ./clase1/part_dinamico_first_fit
echo ""

# Worst Fit
echo "--- WORST FIT ---"
gcc -o clase1/part_dinamico_worst_fit clase1/part_dinamico_worst_fit.c && ./clase1/part_dinamico_worst_fit
echo ""

echo "=========================================="
echo "PRUEBAS COMPLETADAS"
echo "=========================================="
