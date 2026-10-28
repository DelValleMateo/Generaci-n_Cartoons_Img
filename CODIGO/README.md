# Práctico Final Integrador – Computación Avanzada (Cartoon)

> Esta es la documentación técnica original de `CODIGO`, conservada como referencia para Linux/WSL. El [README principal](../README.md) describe el estado verificado de la entrega y el entorno Windows con MSYS2 y Microsoft MPI utilizado para las mediciones.

Genera una imagen estilo *cartoon* a partir de una imagen RGB:

1. **Bordes:** escala de grises → filtro de promedio (3x3 o 5x5) → Sobel → umbral.
2. **Posterizado:** cada canal R, G y B se reduce a 3 o 9 niveles.
3. **Resultado:** se suma el posterizado con los bordes.

Este README explica cómo instalar todo, compilar, comprobar que funciona y hacer las mediciones.

## Archivos

| Archivo | Qué es |
|---|---|
| `punto1_secuencial.cpp` | Punto 1: versión secuencial (la referencia) |
| `punto2_mpi.cpp` | Punto 2: versión MPI (reparte bloques de filas con halo) |
| `punto3_openmp.cpp` | Punto 3: versión OpenMP (`parallel for` en cada etapa) |
| `punto4_hibrido.cpp` | Punto 4: híbrido MPI + OpenMP |
| `probar.sh` | Compila todo y comprueba que las versiones paralelas den **exactamente** lo mismo que el secuencial |

Formato de imagen: **PPM P6** (sin librerías externas). Se convierte desde JPG/PNG con ImageMagick (ver más abajo).

---

## 1. Instalación

### Opción A: WSL2 con Ubuntu (alternativa no usada para las mediciones publicadas)

1. Abrir PowerShell como administrador y ejecutar `wsl --install -d Ubuntu`. Reiniciar la PC cuando lo pida.
2. Comprobar que anda: `wsl -d Ubuntu -u root -- echo hola` debe imprimir `hola`.
   - Si da el error `E_UNEXPECTED`: reiniciar de nuevo y, si sigue, activar la virtualización (**SVM** en AMD / **Intel VT-x**) en la BIOS.
3. Abrir Ubuntu (`wsl -d Ubuntu`; la primera vez pide crear usuario y contraseña) e instalar:

```bash
sudo apt update
sudo apt install -y g++ openmpi-bin libopenmpi-dev imagemagick python3
```

4. Verificar:

```bash
g++ --version
mpicxx --version
mpirun --version
magick --version || convert --version
```

### Opción B: Linux (Ubuntu/Debian)

Mismo comando de instalación del paso 3 de arriba.

### Opción C: sin MPI (solo punto 1 y 3)

Con MSYS2 o MinGW alcanza `g++` con soporte de OpenMP (`-fopenmp`) para los puntos 1 y 3. Las versiones MPI e híbrida también se probaron en Windows con Microsoft MPI; ver el README principal para ese entorno.

---

## 2. Ir a la carpeta del código

En WSL los discos de Windows están en `/mnt/`:

```bash
cd /ruta/al/repositorio/CODIGO
ls
```

Tienen que verse los cuatro archivos `punto*.cpp`, `probar.sh` y este README.

> Consejo: si compilan desde `/mnt/e/...` y es lento, copien la carpeta al home de Linux (`cp -r . ~/tp && cd ~/tp`).

---

## 3. Compilar

```bash
g++    -O2 -Wall -Wextra           -o sec punto1_secuencial.cpp
mpicxx -O2 -Wall -Wextra           -o mpi punto2_mpi.cpp
g++    -O2 -Wall -Wextra -fopenmp  -o omp punto3_openmp.cpp
mpicxx -O2 -Wall -Wextra -fopenmp  -o hib punto4_hibrido.cpp
```

No debería aparecer ningún warning ni error.

---

## 4. Comprobar que funciona

```bash
bash probar.sh
```

Compara cada versión paralela contra el secuencial con `cmp` (salidas idénticas byte a byte), con distintos filtros, niveles, hilos y procesos. Cada línea dice `OK` o `FALLA`. Al final tiene que decir **`TODO OK`**.

Hay que fijarse que aparezcan las líneas `mpi` e `hibrido`. Si dice que no encontró `mpicxx`/`mpirun`, MPI no quedó instalado y esas versiones **no se probaron**.

Con una imagen propia: `bash probar.sh foto.ppm`.

---

## 5. Uso de los programas

```bash
# Secuencial
./sec entrada.ppm salida.ppm [filtro=3] [niveles=3] [umbral=150] [bordes_oscuros=0]

# OpenMP (último argumento: cantidad de hilos)
./omp entrada.ppm salida.ppm [filtro] [niveles] [umbral] [bordes_oscuros] [hilos=4]

# MPI
mpirun -np PROCESOS ./mpi entrada.ppm salida.ppm [filtro] [niveles] [umbral] [bordes_oscuros]

# Híbrido (procesos MPI × hilos OpenMP por proceso)
mpirun -np PROCESOS ./hib entrada.ppm salida.ppm [filtro] [niveles] [umbral] [bordes_oscuros] [hilos=2]
```

| Argumento | Valores | Significado |
|---|---|---|
| `filtro` | 3 o 5 | Tamaño del filtro de promedio |
| `niveles` | 3 o 9 | Niveles de posterizado |
| `umbral` | 0–255 (150 por defecto) | Umbral de detección de bordes |
| `bordes_oscuros` | 0 o 1 | 0 = bordes se suman (blancos), 1 = se restan (contorno negro) |

Cada programa imprime el **tiempo de procesamiento** (sin leer ni guardar el archivo). En MPI e híbrido incluye el reparto y la recolección de filas.

---

## 6. Preparar las imágenes

```bash
magick foto.jpg -resize 800x800!   img800.ppm
magick foto.jpg -resize 2000x2000! img2000.ppm
magick foto.jpg -resize 5000x5000! img5000.ppm
```

(Si `magick` no existe, usar `convert` en lugar de `magick`.)

Para ver un resultado: `magick salida.ppm salida.png` y abrir el PNG. Tiene que verse con colores planos y contornos en los cambios fuertes.

---

## 7. Mediciones

Hay que medir **12 configuraciones**: 3 tamaños (800, 2000, 5000) × 2 filtros (3, 5) × 2 posterizados (3, 9), en las cuatro versiones y con distinta cantidad de hilos/procesos.

Recomendaciones:

- Repetir cada caso **3 veces** y tomar el promedio.
- Cerrar programas pesados mientras se mide.
- Probar 1, 2, 4, 6 y 12 hilos/procesos (según los núcleos de la PC; ver `nproc`). Pasar la cantidad de procesadores lógicos no debería mejorar.
- Si MPI se queja de "not enough slots", agregar `--oversubscribe` a `mpirun` (OpenMPI).
- Para el híbrido, probar varias combinaciones (por ejemplo 2×2, 2×4, 4×2) y comparar.

Ejemplo de medición en bucle (secuencial y OpenMP):

```bash
for img in 800 2000 5000; do
  for f in 3 5; do
    for n in 3 9; do
      echo "== ${img}x${img}  filtro $f  niveles $n"
      ./sec img$img.ppm /tmp/o.ppm $f $n 150 0
      for h in 1 2 4 6 12; do ./omp img$img.ppm /tmp/o.ppm $f $n 150 0 $h; done
    done
  done
done
```

### Fórmulas

- **Speedup** = T_secuencial / T_paralelo
- **Eficiencia** = Speedup / cantidad de procesadores

Se comparan siempre contra el secuencial del **mismo caso** (mismo tamaño, filtro y niveles).

### Tabla para anotar

| Tamaño | Filtro | Niveles | Versión | Procesos | Hilos | Tiempo (s) | Speedup | Eficiencia |
|---|---|---|---|---|---|---|---|---|
| 800x800 | 3 | 3 | Secuencial | 1 | 1 | | 1.00 | 1.00 |
| 800x800 | 3 | 3 | OpenMP | 1 | 4 | | | |
| 800x800 | 3 | 3 | MPI | 4 | 1 | | | |
| 800x800 | 3 | 3 | Híbrido | 2 | 2 | | | |

(Se completa en el Google Doc del grupo.)

Después de medir, comparar una salida paralela con la del secuencial (`cmp`) para confirmar que siguen idénticas.

---

## 8. Cosas a tener en cuenta para el informe

- En imágenes pequeñas, el costo de crear hilos y comunicar en MPI puede reducir la ganancia. Los tiempos medidos están en `../resultados/matriz_completa/resumen.csv`.
- El tiempo medido **excluye** leer y guardar la imagen, y en MPI/híbrido **incluye** la comunicación (reparto y recolección de filas).
- **Halo en MPI:** cada proceso recibe filas extra de sus vecinos: 2 por lado con filtro 3x3 y 3 con 5x5 (k/2 del filtro + 1 de Sobel). El posterizado no necesita halo.
- **OpenMP:** el `parallel for` tiene una barrera implícita al final, así cada etapa empieza cuando la anterior terminó en todos los hilos.
- Justificar las decisiones: tratamiento de los bordes de la imagen, umbral usado, bordes blancos u oscuros.

---

## 9. Subir a git

```bash
cd ..            # carpeta TP FINAL (donde está el .git)
git add CODIGO
git commit -m "Descripción del cambio"
git push
```

---

## Problemas frecuentes

| Problema | Solución |
|---|---|
| `mpicxx: command not found` | Falta instalar `openmpi-bin libopenmpi-dev` (paso 1) |
| `wsl` da `E_UNEXPECTED` | Reiniciar y activar virtualización en la BIOS |
| MPI: "not enough slots available" | Agregar `--oversubscribe` a `mpirun` |
| `magick: command not found` | Usar `convert` o instalar `imagemagick` |
| `bash: probar.sh: Permission denied` | Ejecutarlo con `bash probar.sh` |
| `^M: bad interpreter` / errores raros en el `.sh` | El archivo quedó con saltos de línea de Windows: `sed -i 's/\r$//' probar.sh` |
| Tiempos muy distintos entre corridas | Repetir y promediar; cerrar otros programas |
