# Práctico Final Integrador — Computación Avanzada

**Universidad Autónoma de Entre Ríos (UADER)** · **Integrantes:** Luca Saboredo y Mateo del Valle

Este proyecto genera imágenes de estilo cartoon a partir de imágenes RGB. Incluye cuatro implementaciones del mismo algoritmo: secuencial, MPI, OpenMP e híbrida MPI + OpenMP. Los programas trabajan con imágenes PPM binarias P6; las fotos y salidas PNG de [ejemplos](ejemplos/) sirven como evidencia visual.

## Cómo funciona

La rama de contornos convierte RGB a gris, aplica un borroneado por promedio de 3×3 o 5×5, calcula el gradiente de Sobel y lo umbraliza. En paralelo, la rama de color posteriza cada canal RGB a 3 o 9 niveles. La salida combina el color posterizado con los contornos: pueden sumarse como bordes blancos o restarse como bordes negros.

| Versión | Fuente | Paralelismo |
|---|---|---|
| Secuencial | [punto1_secuencial.cpp](CODIGO/punto1_secuencial.cpp) | Referencia de equivalencia |
| MPI | [punto2_mpi.cpp](CODIGO/punto2_mpi.cpp) | Bloques de filas y halos entre procesos |
| OpenMP | [punto3_openmp.cpp](CODIGO/punto3_openmp.cpp) | Bucles paralelos con memoria compartida |
| Híbrida | [punto4_hibrido.cpp](CODIGO/punto4_hibrido.cpp) | Procesos MPI e hilos OpenMP por proceso |

## Entorno y compilación

Las pruebas entregadas se hicieron en **Windows, MSYS2 UCRT64, GCC 16.2, OpenMP y Microsoft MPI 10.1.3**. Se necesita MSYS2 UCRT64 con `g++`, `mpicxx`, Python 3, y Microsoft MPI SDK y Runtime con `mpiexec` disponible en `PATH`. Para regenerar gráficos se necesita `matplotlib`; para convertir JPG/PNG a PPM se puede usar ImageMagick. Los programas C++ no requieren librerías de imágenes.

Abrir la terminal **MSYS2 UCRT64** en la raíz del repositorio. Comprobar `g++ --version`, `mpicxx --version` y `mpiexec -help`. Con las herramientas instaladas y en `PATH`:

```sh
mkdir -p bin
g++ -O2 -std=c++17 -Wall -Wextra CODIGO/punto1_secuencial.cpp -o bin/secuencial.exe
mpicxx -O2 -std=c++17 -Wall -Wextra CODIGO/punto2_mpi.cpp -o bin/mpi.exe
g++ -O2 -std=c++17 -Wall -Wextra -fopenmp CODIGO/punto3_openmp.cpp -o bin/openmp.exe
mpicxx -O2 -std=c++17 -Wall -Wextra -fopenmp CODIGO/punto4_hibrido.cpp -o bin/hibrido.exe
```

## Generar una imagen

Convertir primero la foto a PPM P6 de 8 bits por canal, por ejemplo `magick foto.jpg -depth 8 entrada.ppm`. La entrada, la salida y los parámetros se pasan en este orden: `entrada salida filtro niveles umbral bordes_oscuros` y, para OpenMP e híbrido, `hilos`. `filtro` admite 3 o 5; `niveles`, 3 o 9; `umbral`, de 0 a 2040; `bordes_oscuros`, 0 o 1.

```sh
./bin/secuencial.exe entrada.ppm secuencial.ppm 5 9 150 0
mpiexec -np 4 ./bin/mpi.exe entrada.ppm mpi.ppm 5 9 150 0
./bin/openmp.exe entrada.ppm openmp.ppm 5 9 150 0 8
mpiexec -np 4 ./bin/hibrido.exe entrada.ppm hibrido.ppm 5 9 150 0 2
```

Los `.ppm` y los ejecutables se ignoran en Git. Para visualizar o compartir una salida, convertirla a PNG con `magick secuencial.ppm secuencial.png`.

## Pruebas y resultados

Desde la raíz, `bash CODIGO/probar.sh` compila las cuatro versiones y compara byte a byte sus salidas con la secuencial (40 comparaciones en la ejecución registrada). `python scripts/test_casos_limite.py` verifica casos límite y errores. `python scripts/validar_resultados.py` audita los CSV existentes. Para regenerar **solo** los seis gráficos desde esos CSV: `python scripts/graficos.py` (requiere `matplotlib`). `python scripts/benchmark.py --salida resultados/matriz_completa` repetiría la matriz completa y sobrescribiría los CSV publicados; usar otra carpeta si se quieren hacer nuevas mediciones.

La matriz publicada tiene **396 ejecuciones reales**: 12 cargas de trabajo (tres tamaños × dos filtros × dos niveles), 11 configuraciones por carga y tres repeticiones. Incluye 132 promedios. También se realizaron 260 comparaciones de píxeles con un oráculo independiente y 32 comprobaciones de entradas/salidas inválidas. Se conserva el [registro de validación](resultados/matriz_completa/validacion.txt) y los [logs](resultados/registros/).

Ejemplo de promedios para **5000×5000, filtro 5×5 y 9 niveles**:

| Configuración | Tiempo promedio (s) | Speedup |
|---|---:|---:|
| Secuencial | 0,665927 | 1,00 |
| MPI, 4 procesos | 0,262911 | 2,53 |
| OpenMP, 8 hilos | 0,175667 | 3,79 |
| Híbrida, 4 procesos × 2 hilos | 0,172924 | 3,85 |

Speedup = tiempo secuencial / tiempo paralelo. Eficiencia = speedup / número total de unidades de ejecución (procesos × hilos). Los tiempos impresos excluyen lectura y escritura; MPI e híbrido incluyen distribución y recolección, por lo que la comparación debe interpretarse con ese alcance.

Datos completos: [mediciones.csv](resultados/matriz_completa/mediciones.csv), [resumen.csv](resultados/matriz_completa/resumen.csv) y [seis gráficos](graficos/). La foto y los cuatro resultados se encuentran en [ejemplos](ejemplos/); su atribución está en [FUENTE_FOTO.txt](ejemplos/FUENTE_FOTO.txt).

## Documentación y organización

- [Informe final en PDF](documentacion/informe_final.pdf) y [fuente DOCX](documentacion/informe_final.docx).
- [Guía de defensa](documentacion/guia_defensa.md), [guía para Mateo](documentacion/GUIA_PARA_MATEO.md) y [prompt para continuar](documentacion/PROMPT_MATEO.md).
- [Consigna original](consignas/CA-Practico%20Final-2026.pdf) y [apuntes](consignas/Apuntes.pdf).
- [README técnico de `CODIGO`](CODIGO/README.md), conservado como referencia adicional.

```text
CODIGO/         Cuatro fuentes C++ y prueba de equivalencia
scripts/        Benchmark, validaciones, gráficos y generación del informe
ejemplos/       Foto original, cuatro salidas PNG y atribución
resultados/     Matriz completa y registros de pruebas, incluido el piloto
graficos/       Seis figuras derivadas del resumen validado
documentacion/  Informe y guías de trabajo y defensa
consignas/      Enunciado y apuntes de la materia
bin/            Ejecutables locales ignorados por Git
```
