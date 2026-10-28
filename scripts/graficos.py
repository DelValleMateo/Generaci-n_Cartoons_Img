#!/usr/bin/env python3
"""Seis figuras comparativas derivadas de la matriz completa validada."""
import csv
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from validar_resultados import check

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "resultados" / "matriz_completa" / "resumen.csv"
OUT = ROOT / "graficos"
check()
with DATA.open(newline="", encoding="utf-8") as stream:
    rows = list(csv.DictReader(stream))
lookup = {(r["tamano"], int(r["filtro"]), int(r["niveles"]), r["implementacion"],
           int(r["procesos"]), int(r["hilos"])): r for r in rows}
SIZES = (800, 2000, 5000)
WORKLOADS = ((3, 3), (3, 9), (5, 3), (5, 9))
XLABELS = ("3×3 · 3", "3×3 · 9", "5×5 · 3", "5×5 · 9")
IMPLEMENTATIONS = (("Secuencial", "secuencial", 1, 1),
                   ("OpenMP 8 h", "openmp", 1, 8),
                   ("MPI 4 p", "mpi", 4, 1),
                   ("Híbrido 4×2", "hibrido", 4, 2))
COLORS = ("#222222", "#1f5a78", "#8b4c28", "#5f527f")
MARKERS = ("o", "s", "^", "D")


def value(size, filt, levels, name, processes, threads, field):
    return float(lookup[(f"{size}x{size}", filt, levels, name, processes, threads)][field])


def setup(title, ylabel, xlabel="Filtro · niveles de posterización"):
    fig, axes = plt.subplots(1, 3, figsize=(13.5, 4.2))
    fig.suptitle(title, fontsize=15, fontweight="bold", y=.99)
    for ax, size in zip(axes, SIZES):
        ax.set_title(f"{size} × {size} píxeles", fontsize=11)
        ax.set_ylabel(ylabel)
        ax.set_xlabel(xlabel)
        ax.grid(axis="y", color="#d9d9d9", linewidth=.6)
        ax.spines[["top", "right"]].set_visible(False)
    return fig, axes


def finish(fig, name, note):
    handles, labels = fig.axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=min(4, len(labels)), frameon=False, fontsize=9,
               bbox_to_anchor=(.5, .06))
    fig.text(.5, .015, note, ha="center", fontsize=8, color="#444444")
    fig.tight_layout(rect=(0, .13, 1, .93))
    fig.savefig(OUT / name, dpi=200, facecolor="white")
    plt.close(fig)


def plot_workloads(field, title, ylabel, filename, configurations, note):
    fig, axes = setup(title, ylabel)
    for ax, size in zip(axes, SIZES):
        for i, (label, name, p, h) in enumerate(configurations):
            y = [value(size, f, n, name, p, h, field) for f, n in WORKLOADS]
            ax.plot(range(4), y, label=label, color=COLORS[i], marker=MARKERS[i], linewidth=1.8)
        ax.set_xticks(range(4), XLABELS, rotation=35, ha="right")
        ax.set_ylim(bottom=0)
    finish(fig, filename, note)


def plot_scaling(name, settings, xlabel, title, filename):
    fig, axes = setup(title, "Speedup (veces)", xlabel)
    colors = ("#1f5a78", "#5f527f", "#8b4c28", "#25745a")
    for ax, size in zip(axes, SIZES):
        for i, (f, n) in enumerate(WORKLOADS):
            y = [value(size, f, n, name, p, h, "speedup") for p, h in settings]
            ax.plot(range(len(settings)), y, label=f"{f}×{f}, {n} niveles", color=colors[i],
                    marker=MARKERS[i], linewidth=1.8)
        ax.set_xticks(range(len(settings)), [f"{p}×{h}" if name == "hibrido" else str(h if name == "openmp" else p)
                                             for p, h in settings])
        ax.axhline(1, color="#888888", linestyle="--", linewidth=.8)
        ax.set_ylim(bottom=0)
    finish(fig, filename, "Speedup respecto del promedio secuencial del mismo tamaño, filtro y niveles.")


def main():
    OUT.mkdir(exist_ok=True)
    plot_workloads("tiempo_promedio_s", "Tiempo de procesamiento de las cuatro implementaciones", "Tiempo promedio (s)",
                   "tiempos_comparativos.png", IMPLEMENTATIONS,
                   "Tres ejecuciones por punto. MPI e híbrido incluyen distribución y recolección; lectura y escritura excluidas.")
    plot_scaling("openmp", ((1, 1), (1, 2), (1, 4), (1, 8)), "Hilos OpenMP",
                 "Speedup al aumentar los hilos OpenMP", "speedup_openmp.png")
    plot_scaling("mpi", ((1, 1), (2, 1), (4, 1)), "Procesos MPI",
                 "Speedup al aumentar los procesos MPI", "speedup_mpi.png")
    plot_scaling("hibrido", ((2, 2), (2, 4), (4, 2)), "Procesos × hilos",
                 "Speedup de las configuraciones híbridas", "speedup_hibrido.png")
    plot_workloads("eficiencia", "Eficiencia de las configuraciones paralelas", "Eficiencia (fracción)",
                   "eficiencia.png", IMPLEMENTATIONS[1:],
                   "Eficiencia = speedup / (procesos × hilos). Tres ejecuciones por punto.")
    fig, axes = plt.subplots(1, 2, figsize=(10, 4.2))
    fig.suptitle("Costo relativo del filtro 5×5 frente al 3×3", fontsize=15, fontweight="bold")
    for ax, levels in zip(axes, (3, 9)):
        for i, (label, name, p, h) in enumerate(IMPLEMENTATIONS):
            ratios = [value(size, 5, levels, name, p, h, "tiempo_promedio_s") /
                      value(size, 3, levels, name, p, h, "tiempo_promedio_s") for size in SIZES]
            ax.plot(range(3), ratios, label=label, marker=MARKERS[i], color=COLORS[i], linewidth=1.8)
        ax.set_xticks(range(3), [str(size) for size in SIZES])
        ax.set_title(f"{levels} niveles de posterización")
        ax.set_xlabel("Lado de imagen (píxeles)")
        ax.set_ylabel("Tiempo 5×5 / tiempo 3×3")
        ax.grid(axis="y", color="#d9d9d9", linewidth=.6)
        ax.spines[["top", "right"]].set_visible(False)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=4, frameon=False, fontsize=9,
               bbox_to_anchor=(.5, .04))
    fig.tight_layout(rect=(0, .12, 1, .92))
    fig.savefig(OUT / "coste_filtro.png", dpi=200, facecolor="white")
    plt.close(fig)
    print("6 gráficos creados desde", DATA)


if __name__ == "__main__":
    main()
