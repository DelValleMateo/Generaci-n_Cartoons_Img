#!/usr/bin/env python3
"""Compila, verifica equivalencia y mide las 12 configuraciones de la consigna."""
import argparse
import csv
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FIELDS = ["tamano", "filtro", "niveles", "implementacion", "procesos", "hilos", "repeticion", "segundos", "sha256"]
SUMMARY = ["tamano", "filtro", "niveles", "implementacion", "procesos", "hilos", "tiempo_promedio_s", "speedup", "eficiencia"]


def run(command, timeout=3600):
    result = subprocess.run(command, text=True, capture_output=True, timeout=timeout, cwd=ROOT)
    if result.returncode:
        raise RuntimeError(f"Falló ({result.returncode}): {' '.join(map(str, command))}\n{result.stdout}\n{result.stderr}")
    return result.stdout


def image(path, n):
    # La misma escena determinista para todos los tamaños y algoritmos.
    with path.open("wb") as out:
        out.write(f"P6\n{n} {n}\n255\n".encode())
        for y in range(n):
            row = bytearray(n * 3)
            for x in range(n):
                p = 3 * x
                row[p] = (x * 13 // 37 + y * 7 // 23) % 256
                row[p + 1] = (x * 3 // 11 + y * 17 // 29) % 256
                row[p + 2] = 230 if (x // 80 + y // 80) % 2 else 35
            out.write(row)


def build(folder, require_mpi=True):
    launcher = shutil.which("mpirun") or shutil.which("mpiexec")
    for name in ("g++", "mpicxx"):
        if not shutil.which(name) and (require_mpi or name == "g++"):
            raise RuntimeError(f"Falta {name} en PATH; no se ejecutaron mediciones.")
    if require_mpi and not launcher:
        raise RuntimeError("Falta mpirun o mpiexec en PATH; no se ejecutaron mediciones.")
    commands = {
        "secuencial": ["g++", "-O2", "-std=c++17", "-Wall", "-Wextra", "CODIGO/punto1_secuencial.cpp"],
        "openmp": ["g++", "-O2", "-std=c++17", "-Wall", "-Wextra", "-fopenmp", "CODIGO/punto3_openmp.cpp"],
        "mpi": ["mpicxx", "-O2", "-std=c++17", "-Wall", "-Wextra", "CODIGO/punto2_mpi.cpp"],
        "hibrido": ["mpicxx", "-O2", "-std=c++17", "-Wall", "-Wextra", "-fopenmp", "CODIGO/punto4_hibrido.cpp"],
    }
    for name, command in commands.items():
        target = folder / (name + (".exe" if os.name == "nt" else ""))
        run(command + ["-o", str(target)], timeout=120)
    return {name: folder / (name + (".exe" if os.name == "nt" else "")) for name in commands}, launcher


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repeticiones", type=int, default=3)
    parser.add_argument("--salida", type=Path, default=ROOT / "resultados")
    parser.add_argument("--solo-800", action="store_true", help="Prueba inicial de 800x800")
    args = parser.parse_args()
    if args.repeticiones < 3:
        parser.error("La consigna exige al menos tres repeticiones")
    cpus = os.cpu_count() or 1
    threads = [x for x in (1, 2, 4, 8) if x <= cpus]
    processes = [x for x in (1, 2, 4) if x <= cpus]
    hybrid = [(p, h) for p, h in ((2, 2), (2, 4), (4, 2)) if p * h <= cpus]
    args.salida.mkdir(parents=True, exist_ok=True)
    rows = []
    with tempfile.TemporaryDirectory(prefix="cartoon_bench_") as temp:
        folder = Path(temp)
        executables, launcher = build(folder)
        sizes = (800,) if args.solo_800 else (800, 2000, 5000)
        for size in sizes:
            source = folder / f"entrada_{size}.ppm"
            image(source, size)
            for filt in (3, 5):
                for levels in (3, 9):
                    cfg = (str(filt), str(levels), "150", "0")
                    cases = [("secuencial", 1, 1)]
                    cases += [("openmp", 1, h) for h in threads]
                    cases += [("mpi", p, 1) for p in processes]
                    cases += [("hibrido", p, h) for p, h in hybrid]
                    reference = None
                    for name, p, h in cases:
                        for repetition in range(1, args.repeticiones + 1):
                            target = folder / "salida.ppm"
                            command = ([launcher, "-np", str(p)] if name in ("mpi", "hibrido") else [])
                            command += [str(executables[name]), str(source), str(target), *cfg]
                            if name in ("openmp", "hibrido"):
                                command += [str(h)]
                            output = run(command)
                            match = re.search(r"\btiempo\s+([0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)\s+s\b", output)
                            if not match or not target.is_file():
                                raise RuntimeError(f"Falta tiempo o imagen de salida: {output}")
                            digest_state = hashlib.sha256()
                            with target.open("rb") as image_file:
                                for block in iter(lambda: image_file.read(1024 * 1024), b""):
                                    digest_state.update(block)
                            digest = digest_state.hexdigest()
                            if name == "secuencial":
                                reference = digest
                            elif digest != reference:
                                raise RuntimeError(f"Imagen distinta: {size}, filtro={filt}, niveles={levels}, {name} {p}x{h}")
                            rows.append(dict(zip(FIELDS, (f"{size}x{size}", filt, levels, name, p, h, repetition, match.group(1), digest))))
                            with (args.salida / "mediciones.csv").open("w", newline="", encoding="utf-8") as stream:
                                writer = csv.DictWriter(stream, FIELDS)
                                writer.writeheader()
                                writer.writerows(rows)
                            print(f"{size} {filt} {levels} {name} {p}x{h} rep={repetition} {match.group(1)} s", flush=True)
    grouped = {}
    for row in rows:
        key = tuple(row[field] for field in SUMMARY[:6])
        grouped.setdefault(key, []).append(float(row["segundos"]))
    baselines = {key[:3]: sum(values) / len(values) for key, values in grouped.items() if key[3] == "secuencial"}
    with (args.salida / "resumen.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, SUMMARY)
        writer.writeheader()
        for key, values in grouped.items():
            mean = sum(values) / len(values)
            speedup = baselines[key[:3]] / mean
            units = int(key[4]) * int(key[5])
            writer.writerow(dict(zip(SUMMARY, (*key, f"{mean:.9f}", f"{speedup:.6f}", f"{speedup / units:.6f}"))))
    print("Mediciones y resumen guardados en", args.salida)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
