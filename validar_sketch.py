#!/usr/bin/env python3
"""
Validación del Sketch de Cauchy y del caché de reemplazo directo.
Invoca al ejecutable C++ (cauchy_cli) y guarda un reporte limpio en disco,
sobrescribiendo los resultados anteriores en cada ejecución.
Los resultados corresponden a los solicitados en la actividad 1.2, así que hay 4 pruebas aquí
"""

import argparse
import json
import math
import subprocess
from pathlib import Path


def run_cli(cli_path: str, *args) -> dict:
    cmd = [cli_path, *map(str, args)]
    res = subprocess.run(cmd, capture_output=True, text=True, check=True)
    return json.loads(res.stdout)


def main():
    parser = argparse.ArgumentParser(description="Validación del Sketch de Cauchy.")
    parser.add_argument("--cli", default="./cauchy_cli", help="Ruta al binario cauchy_cli")
    parser.add_argument("--trace", required=True, help="Ruta a la traza binaria (.bin)")
    parser.add_argument("--out-dir", default="resultados", help="Carpeta de salida")
    parser.add_argument("--k", type=int, default=512, help="Número de filas del sketch")
    parser.add_argument("--B", type=int, default=4096, help="Capacidad del caché")
    parser.add_argument("--T", type=float, default=10.0, help="Largo del intervalo (s)")
    parser.add_argument("--sigma", type=int, default=42, help="Semilla del sketch")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # 1. Mediana(|C_ji|) sobre 10^6 entradas
    res_med = run_cli(args.cli, "--mode", "median", "--n", 1_000_000, "--k", args.k, "--sigma", args.sigma)

    # 2. Determinismo (intra-proceso e inter-proceso)
    res_det1 = run_cli(args.cli, "--mode", "determinism", "--sigma", args.sigma, "--j", 1, "--ip", "0xC0A80001", "--n", 10_000)
    res_det2 = run_cli(args.cli, "--mode", "determinism", "--sigma", args.sigma, "--j", 1, "--ip", "0xC0A80001", "--n", 10_000)
    det_ok = bool(res_det1["all_equal"] and res_det2["all_equal"] and res_det1["value"] == res_det2["value"])
    assert det_ok, "Fallo en prueba de determinismo"

    # 3. Una sola IP con cuenta c contra sketch vacío
    c_val = 100_000
    res_single = run_cli(args.cli, "--mode", "single-ip", "--k", args.k, "--sigma", args.sigma, "--ip", "0xC0A80001", "--c", c_val)
    err_rel_single = abs(res_single["L_hat"] - c_val) / c_val
    err_teorico = math.pi / (2.0 * math.sqrt(args.k))

    # 4. Exactitud del caché (B vs B=0) en un par de intervalos de largo T
    res_no_cache = run_cli(args.cli, "--mode", "interval-pair", "--trace", args.trace, "--T", args.T, "--k", args.k, "--sigma", args.sigma, "--B", 0)
    res_cache = run_cli(args.cli, "--mode", "interval-pair", "--trace", args.trace, "--T", args.T, "--k", args.k, "--sigma", args.sigma, "--B", args.B)

    L0 = res_no_cache["L_hat"]
    LB = res_cache["L_hat"]
    diff_rel_cache = abs(LB - L0) / L0 if L0 != 0 else abs(LB - L0)

    # Resumen estructurado (útil para tablas o gráficos posteriores)
    summary = {
        "params": {"k": args.k, "B": args.B, "T": args.T, "sigma": args.sigma, "trace": args.trace},
        "prueba_1_mediana": res_med,
        "prueba_2_determinismo": {"ok": det_ok, "value": res_det1["value"]},
        "prueba_3_single_ip": {
            **res_single,
            "error_relativo": err_rel_single,
            "error_teorico_esperado": err_teorico,
        },
        "prueba_4_cache": {
            "sin_cache": res_no_cache,
            "con_cache": res_cache,
            "diferencia_relativa": diff_rel_cache,
        },
    }

    # Reporte legible en texto plano
    lines = [
        "=== REPORTE DE VALIDACIÓN: SKETCH DE CAUCHY Y CACHÉ ===",
        f"Traza: {args.trace} | k={args.k} | B={args.B} | T={args.T} s | sigma={args.sigma}",
        "",
        "[Prueba 1] Mediana(|C_ji|)",
        f"  Entradas evaluadas : {res_med['n']:,}",
        f"  Mediana obtenida   : {res_med['median']:.6f} (esperado ~1.0)",
        "",
        "[Prueba 2] Determinismo de Entrada(sigma, j, i)",
        f"  Estado             : {'OK' if det_ok else 'FALLO'}",
        f"  Valor (42, 1, IP)  : {res_det1['value']:.12f}",
        "",
        "[Prueba 3] Una sola IP contra sketch vacío",
        f"  Cuenta real (c)    : {c_val:,}",
        f"  Estimación (L_hat) : {res_single['L_hat']:,.4f}",
        f"  Error relativo     : {err_rel_single:.4%} (típico teórico ~{err_teorico:.2%})",
        "",
        f"[Prueba 4] Exactitud del caché (T = {args.T} s, N1 = {res_cache['N1']:,}, N2 = {res_cache['N2']:,})",
        f"  Sin caché (B=0)    : L_hat = {L0:,.6f} | updates = {res_no_cache['updates']:,} | phi_0 = {res_no_cache['phi_B']:.6f}",
        f"  Con caché (B={args.B}) : L_hat = {LB:,.6f} | updates = {res_cache['updates']:,} | phi_B = {res_cache['phi_B']:.6f}",
        f"  Diferencia relativa: {diff_rel_cache:.3e}",
        "",
    ]
    report_text = "\n".join(lines)

    # Sobrescribir archivos de salida ("w" trunca el archivo anterior por completo)
    (out_dir / "validacion.txt").write_text(report_text, encoding="utf-8")
    (out_dir / "validacion.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(report_text)
    print(f"Resultados guardados en: {out_dir}/validacion.txt y {out_dir}/validacion.json")


if __name__ == "__main__":
    main()