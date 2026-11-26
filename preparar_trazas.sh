#!/usr/bin/env bash
# ==============================================================================
# preparar_trazas.sh
# Compila pcap2bin y descarga/convierte las trazas MAWI indicadas a data/*.bin.
# Si una traza .bin ya existe, la omite.
#
# Uso:
#   ./preparar_trazas.sh 201801261400
#   ./preparar_trazas.sh 201801251400 201801261400 201801301400
# ==============================================================================

set -euo pipefail

BASE_URL="https://mawi.wide.ad.jp/mawi/samplepoint-F/2018"
DATA_DIR="data"

if [[ $# -eq 0 ]]; then
    echo "Uso: $0 <ID_TRAZA> [ID_TRAZA ...]" >&2
    exit 1
fi

# Compilar pcap2bin si no existe o si pcap2bin.cpp fue modificado
if [[ ! -x ./pcap2bin || pcap2bin.cpp -nt ./pcap2bin ]]; then
    echo "[preparar_trazas] Compilando pcap2bin.cpp..."
    g++ -O2 -march=native -o pcap2bin pcap2bin.cpp
fi

mkdir -p "$DATA_DIR"

for ID in "$@"; do
    OUT_BIN="${DATA_DIR}/${ID}.bin"
    TMP_BIN="${OUT_BIN}.tmp"
    URL="${BASE_URL}/${ID}.pcap.gz"

    if [[ -s "$OUT_BIN" ]]; then
        echo "[preparar_trazas] '$OUT_BIN' ya existe. Omitiendo descarga."
        continue
    fi

    echo "[preparar_trazas] Descargando y convirtiendo ${ID}.pcap.gz -> ${OUT_BIN}..."
    # Limpiar .tmp si el usuario cancela con Ctrl+C o falla la descarga
    trap 'rm -f "$TMP_BIN"' EXIT INT TERM

    curl -fSL "$URL" | zcat | ./pcap2bin > "$TMP_BIN"
    mv "$TMP_BIN" "$OUT_BIN"

    trap - EXIT INT TERM
    echo "[preparar_trazas] Listo: $OUT_BIN"
done