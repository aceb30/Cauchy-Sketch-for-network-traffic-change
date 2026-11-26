#!/usr/bin/env bash
# ==============================================================================
# reproducir.sh
# Pipeline completa: prepara las trazas binarias, compila el código C++,
# limpia resultados previos y ejecuta las pruebas de validación.
# ==============================================================================

set -euo pipefail

# Configuración de rutas y parámetros
TRACE_ID="201801261400"
TRACE_BIN="data/${TRACE_ID}.bin"
OUT_DIR="resultados"
K=512
B=4096
T=10
SIGMA=42

# Preparación de trazas (descarga + conversión si faltan) 
echo "[1/5] Verificando trazas binarias..."
./preparar_trazas.sh "$TRACE_ID"
# Para la Act. 2.2, basta cambiar la línea anterior por:
# ./preparar_trazas.sh 201801251400 201801261400 201801301400

# Compilación de binarios C++
echo "[2/5] Compilando cauchy_cli.cpp..."
g++ -O3 -march=native -std=c++17 cauchy_cli.cpp -o cauchy_cli
g++ -O3 -march=native -std=c++17 exact_ref.cpp -o exact_ref

# Limpieza de resultados previos 
echo "[3/5] Reiniciando directorio de salida '$OUT_DIR/'..."
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

# Ejecución de validación 
echo "[4/5] Ejecutando pruebas de validación..."
python3 validar_sketch.py \
    --cli ./cauchy_cli \
    --trace "$TRACE_BIN" \
    --out-dir "$OUT_DIR" \
    --k "$K" \
    --B "$B" \
    --T "$T" \
    --sigma "$SIGMA"

echo "Proceso completado con éxito."

# Referencia exacta por intervalos 
echo "[5/5] Calculando referencia exacta (N_q, L_q, r_q)..."
./exact_ref --trace "$TRACE_BIN" --T "$T" > "${OUT_DIR}/referencia_exacta.csv"