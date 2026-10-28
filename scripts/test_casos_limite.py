#!/usr/bin/env python3
"""Pruebas independientes de píxeles, reparto MPI y entradas inválidas."""
from pathlib import Path
import re
import subprocess
import tempfile
from benchmark import build, ROOT


def reference(rgb, width, height, filt, levels, threshold=150):
    size = width * height
    gray = [int(.30 * rgb[3*i] + .59 * rgb[3*i+1] + .11 * rgb[3*i+2]) for i in range(size)]
    blurred = [0] * size
    radius = filt // 2
    for y in range(height):
        for x in range(width):
            neighbors = [gray[yy*width+xx]
                         for yy in range(max(0, y-radius), min(height, y+radius+1))
                         for xx in range(max(0, x-radius), min(width, x+radius+1))]
            blurred[y*width+x] = sum(neighbors) // len(neighbors)
    edges = [0] * size
    for y in range(1, height-1):
        for x in range(1, width-1):
            def v(dy, dx): return blurred[(y+dy)*width+x+dx]
            sx = v(-1,1)+2*v(0,1)+v(1,1)-v(-1,-1)-2*v(0,-1)-v(1,-1)
            sy = v(1,-1)+2*v(1,0)+v(1,1)-v(-1,-1)-2*v(-1,0)-v(-1,1)
            edges[y*width+x] = 255 if abs(sx)+abs(sy) > threshold else 0
    result = bytearray(len(rgb))
    for i, value in enumerate(rgb):
        quantized = int(((value * levels // 256) + .5) * 256.0 / levels)
        result[i] = min(255, quantized + edges[i//3])
    return b"P6\n%d %d\n255\n" % (width, height) + result


def execute(command):
    return subprocess.run(command, cwd=ROOT, text=True, capture_output=True, timeout=20)


def main():
    with tempfile.TemporaryDirectory(prefix="cartoon_edge_") as temp:
        folder = Path(temp)
        binaries, launcher = build(folder)
        comparisons = 0
        for width, height in ((1,1), (2,2), (3,5), (7,5), (9,4)):
            rgb = bytes((x*31 + y*17 + c*53 + (x*y*7)) % 256
                        for y in range(height) for x in range(width) for c in range(3))
            source = folder / "input.ppm"
            source.write_bytes(b"P6\n%d %d\n255\n" % (width, height) + rgb)
            for filt in (3,5):
                for levels in (3,9):
                    expected = reference(rgb, width, height, filt, levels)
                    cases = [("secuencial",1,1)]
                    cases += [("openmp",1,h) for h in (1,2,4,8)]
                    cases += [("mpi",p,1) for p in (1,2,3,4,7,8)]
                    cases += [("hibrido",p,h) for p,h in ((2,2),(4,2))]
                    for name,p,h in cases:
                        target = folder / "output.ppm"
                        command = ([launcher,"-np",str(p)] if name in ("mpi","hibrido") else [])
                        command += [str(binaries[name]),str(source),str(target),str(filt),str(levels),"150","0"]
                        if name in ("openmp","hibrido"):
                            command.append(str(h))
                        done = execute(command)
                        assert done.returncode == 0, (name,width,height,filt,levels,done.stderr)
                        if name in ("mpi", "hibrido"):
                            assert f"procesos {p}" in done.stdout, (name,p,done.stdout)
                        if name == "openmp":
                            assert f"hilos efectivos {h}" in done.stdout, (name,h,done.stdout)
                        if name == "hibrido":
                            assert f"hilos efectivos {h}..{h}" in done.stdout, (name,h,done.stdout)
                        assert target.read_bytes() == expected, (name,width,height,filt,levels)
                        comparisons += 1
        invalid = [b"P3\n1 1\n255\n0 0 0", b"P6\n0 1\n255\n", b"P6\n1 1\n100\n\x00\x00\x00",
                   b"P6\n1 1\n255\n\x00", b"P6\n-1 1\n255\n", b"P6\n50000 50000\n255\n"]
        failures_checked = 0
        for payload in invalid:
            source.write_bytes(payload)
            for name in binaries:
                command = ([launcher,"-np","2"] if name in ("mpi","hibrido") else [])
                command += [str(binaries[name]),str(source),str(folder / "invalid.ppm")]
                done = execute(command)
                assert done.returncode != 0, (name,payload)
                failures_checked += 1
        source.write_bytes(b"P6\n1 1\n255\n\x10\x20\x30")
        for name in binaries:
            command = ([launcher,"-np","2"] if name in ("mpi","hibrido") else [])
            command += [str(binaries[name]),str(source),str(folder / "output.ppm"),"7","3"]
            if name in ("openmp","hibrido"):
                command.append("2")
            done = execute(command)
            assert done.returncode != 0, (name,"invalid filter")
            failures_checked += 1
        for name in binaries:
            command = ([launcher,"-np","2"] if name in ("mpi","hibrido") else [])
            command += [str(binaries[name]),str(source),str(folder),"3","3","150","0"]
            if name in ("openmp","hibrido"):
                command.append("2")
            done = execute(command)
            assert done.returncode != 0, (name,"output is a directory")
            failures_checked += 1
        print(f"OK: {comparisons} comparaciones de píxeles contra referencia independiente")
        print(f"OK: {failures_checked} rechazos de entradas o parámetros inválidos")


if __name__ == "__main__":
    main()
