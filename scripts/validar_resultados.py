#!/usr/bin/env python3
"""Audita integridad, repeticiones, equivalencia y fórmulas de los CSV medidos."""
import csv
from collections import defaultdict
from decimal import Decimal
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "resultados" / "matriz_completa"
KEY = ("tamano", "filtro", "niveles", "implementacion", "procesos", "hilos")
CONFIGS = {("secuencial", "1", "1"),
           *(("openmp", "1", str(h)) for h in (1, 2, 4, 8)),
           *(("mpi", str(p), "1") for p in (1, 2, 4)),
           *(("hibrido", str(p), str(h)) for p, h in ((2, 2), (2, 4), (4, 2)))}


def read(name):
    with (DATA / name).open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def check():
    measurements, summary = read("mediciones.csv"), read("resumen.csv")
    assert len(measurements) == 396, f"Mediciones: {len(measurements)}"
    assert len(summary) == 132, f"Resúmenes: {len(summary)}"
    expected_cases = {(f"{n}x{n}", str(f), str(l)) for n in (800, 2000, 5000)
                      for f in (3, 5) for l in (3, 9)}
    groups = defaultdict(list)
    hashes = defaultdict(set)
    for row in measurements:
        key = tuple(row[field] for field in KEY)
        assert key[:3] in expected_cases
        assert key[3:] in CONFIGS
        assert row["repeticion"] in ("1", "2", "3")
        assert Decimal(row["segundos"]) > 0
        assert len(row["sha256"]) == 64
        groups[key].append(row)
        hashes[key[:3]].add(row["sha256"])
    assert len(groups) == 132
    assert all(sorted(r["repeticion"] for r in rows) == ["1", "2", "3"] for rows in groups.values())
    assert len(hashes) == 12 and all(len(values) == 1 for values in hashes.values())
    means = {key: sum(Decimal(r["segundos"]) for r in rows) / 3 for key, rows in groups.items()}
    baselines = {key[:3]: value for key, value in means.items() if key[3] == "secuencial"}
    assert len(baselines) == 12
    seen = set()
    for row in summary:
        key = tuple(row[field] for field in KEY)
        assert key in groups and key not in seen
        seen.add(key)
        mean = Decimal(row["tiempo_promedio_s"])
        speedup = Decimal(row["speedup"])
        efficiency = Decimal(row["eficiencia"])
        units = int(row["procesos"]) * int(row["hilos"])
        assert abs(mean - means[key]) <= Decimal("0.000000001")
        assert abs(speedup - baselines[key[:3]] / means[key]) <= Decimal("0.000001")
        assert abs(efficiency - (baselines[key[:3]] / means[key]) / units) <= Decimal("0.000001")
    assert len(seen) == 132
    return ("VALIDACIÓN APROBADA\n"
            "12 casos de imagen/filtro/niveles; 11 configuraciones por caso.\n"
            "396 mediciones: tres repeticiones únicas por configuración.\n"
            "132 promedios, speedups y eficiencias recalculados contra su referencia secuencial.\n"
            "Salidas equivalentes: un único SHA-256 por cada uno de los 12 casos.\n")


if __name__ == "__main__":
    result = check()
    (DATA / "validacion.txt").write_text(result, encoding="utf-8")
    print(result, end="")
