#!/bin/bash
# Compila las 4 versiones y comprueba que openmp / mpi / híbrido den EXACTAMENTE lo mismo que el secuencial.
# Uso:  ./probar.sh [imagen.ppm]      (sin argumento genera una imagen de prueba de 800x800)
# Las fuentes deben estar en la misma carpeta: punto1_secuencial.cpp, punto3_openmp.cpp, punto2_mpi.cpp, punto4_hibrido.cpp
cd "$(dirname "$0")" || exit 1
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
FALLAS=0

IMG="$1"
if [ -z "$IMG" ]; then
    IMG="$TMP/prueba.ppm"
    python3 - "$IMG" <<'PY'
import sys, random
random.seed(1); n = 800
with open(sys.argv[1], "wb") as f:
    f.write(b"P6\n%d %d\n255\n" % (n, n))
    for i in range(n):
        fila = bytearray()
        for j in range(n):
            if (i // 100 + j // 100) % 2: r, g, b = 220, 60, 40
            else: r, g, b = 30, 90, 200
            if (i - 400) ** 2 + (j - 400) ** 2 < 150 ** 2: r, g, b = 240, 230, 60
            fila += bytes((min(255, max(0, r + random.randint(-8, 8))), min(255, max(0, g + random.randint(-8, 8))), min(255, max(0, b + random.randint(-8, 8)))))
        f.write(fila)
PY
fi
[ -f "$IMG" ] || { echo "No existe la imagen: $IMG"; exit 1; }

echo "== Compilando =="
g++ -O2 -Wall -Wextra -o "$TMP/sec" punto1_secuencial.cpp || { echo "FALLA compilar secuencial"; exit 1; }
g++ -O2 -Wall -Wextra -fopenmp -o "$TMP/omp" punto3_openmp.cpp || { echo "FALLA compilar openmp"; exit 1; }
HAY_MPI=0
if command -v mpicxx >/dev/null && command -v mpirun >/dev/null; then
    mpicxx -O2 -Wall -Wextra -o "$TMP/mpi" punto2_mpi.cpp && mpicxx -O2 -Wall -Wextra -fopenmp -o "$TMP/hib" punto4_hibrido.cpp && HAY_MPI=1
    [ $HAY_MPI = 1 ] || { echo "FALLA compilar MPI/híbrido"; FALLAS=$((FALLAS+1)); }
else
    echo "(mpicxx/mpirun no encontrados: se saltan MPI e híbrido)"
fi

verificar() {  # $1 = nombre de la prueba; $2 = archivo a comparar con la referencia
    if cmp -s "$TMP/ref.ppm" "$2"; then echo "OK     $1"; else echo "FALLA  $1"; FALLAS=$((FALLAS+1)); fi
}

echo "== Comparando contra el secuencial =="
for cfg in "3 3 150 0" "3 9 150 0" "5 3 150 1" "5 9 150 1"; do
    "$TMP/sec" "$IMG" "$TMP/ref.ppm" $cfg > /dev/null || { echo "FALLA secuencial $cfg"; FALLAS=$((FALLAS+1)); continue; }
    for h in 1 2 4; do
        "$TMP/omp" "$IMG" "$TMP/o.ppm" $cfg $h > /dev/null; verificar "openmp  [$cfg] hilos=$h" "$TMP/o.ppm"
    done
    if [ $HAY_MPI = 1 ]; then
        for p in 1 2 3 4 7; do
            mpirun --oversubscribe -np $p "$TMP/mpi" "$IMG" "$TMP/m.ppm" $cfg > /dev/null; verificar "mpi     [$cfg] procesos=$p" "$TMP/m.ppm"
        done
        for p in 2 3; do
            mpirun --oversubscribe -np $p "$TMP/hib" "$IMG" "$TMP/h.ppm" $cfg 2 > /dev/null; verificar "hibrido [$cfg] procesos=$p hilos=2" "$TMP/h.ppm"
        done
    fi
done

echo
[ $FALLAS = 0 ] && echo "TODO OK" || echo "HAY $FALLAS FALLAS"
exit $FALLAS
