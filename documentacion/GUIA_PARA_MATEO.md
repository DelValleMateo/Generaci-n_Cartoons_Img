# Guía para Mateo — estado final del práctico

Mateo: este repositorio reúne el Práctico Final Integrador de Computación Avanzada de UADER que desarrollamos con Luca Saboredo. La guía principal es el [README de la raíz](../README.md). El estado compartido debe tomarse siempre de `main` en [GitHub](https://github.com/DelValleMateo/Generaci-n_Cartoons_Img).

## Dónde está cada cosa

- `CODIGO/`: tus cuatro versiones C++ (`punto1_secuencial.cpp`, `punto2_mpi.cpp`, `punto3_openmp.cpp`, `punto4_hibrido.cpp`) y `probar.sh`. El [README técnico](../CODIGO/README.md) conserva tus explicaciones para Linux/WSL; el README raíz documenta el entorno Windows utilizado para la entrega.
- `scripts/`: benchmark, pruebas de casos límite, auditoría de resultados, gráficos y generación/exportación del informe.
- `resultados/matriz_completa/`: las 396 mediciones originales, 132 promedios y la validación. `resultados/registros/` contiene los logs y el piloto de 800×800.
- `graficos/`: seis comparaciones creadas desde los CSV validados.
- `ejemplos/`: foto de referencia y cuatro cartoons PNG. `FUENTE_FOTO.txt` consigna autor y licencia.
- `documentacion/`: informe final [PDF](informe_final.pdf) y [DOCX](informe_final.docx), [guía de defensa](guia_defensa.md) y el [prompt para tu IA](PROMPT_MATEO.md).
- `consignas/`: enunciado original y apuntes.

## Qué se hizo y qué se comprobó

Se preservó el algoritmo de las cuatro implementaciones. Los cambios de código respecto de la versión inicial agregaron validación de parámetros y entradas/salidas, mayor control de errores y reporte de hilos efectivos; `probar.sh` ahora exige disponer de MPI para declarar aprobadas las cuatro versiones. La organización final movió consignas y registros, y completó documentación; no cambió las fórmulas del filtro ni los resultados medidos.

Las pruebas previas a esta organización incluyeron 396 ejecuciones reales (12 cargas × 11 configuraciones × 3 repeticiones), 40 comparaciones byte a byte, 260 comparaciones de píxeles con un oráculo independiente y 32 casos de error. Los CSV completos están en [mediciones.csv](../resultados/matriz_completa/mediciones.csv) y [resumen.csv](../resultados/matriz_completa/resumen.csv). Los tiempos dependen del equipo: como ejemplo, para 5000×5000, filtro 5×5 y 9 niveles, los promedios fueron 0,665927 s secuencial; 0,262911 s MPI con 4 procesos; 0,175667 s OpenMP con 8 hilos; 0,172924 s híbrido 4×2. Los seis gráficos y el informe explican el resto.

Para repetir las comprobaciones sin recalcular la matriz completa: `bash CODIGO/probar.sh`, `python scripts/test_casos_limite.py` y `python scripts/validar_resultados.py`. Para regenerar las figuras existentes: `python scripts/graficos.py`. Estos comandos se ejecutan desde la raíz.

## Actualizar tu copia y ejecutar

Si todavía no tenés la carpeta: `git clone https://github.com/DelValleMateo/Generaci-n_Cartoons_Img.git`. Si ya la tenés, guardá o confirmá primero tus modificaciones locales, abrí una terminal en la raíz, ejecutá `git status`, `git switch main` y `git pull --ff-only origin main`. Si `pull --ff-only` se detiene, revisá la divergencia antes de integrar; no fuerces ni reemplaces el trabajo local.

En Windows, usá **MSYS2 UCRT64**, GCC con OpenMP, el SDK y Runtime de **Microsoft MPI 10.1.3**, `mpicxx` y `mpiexec` en `PATH`. En el [README principal](../README.md#entorno-y-compilación) están los cuatro comandos exactos de compilación y los cuatro de ejecución. `bin/` se crea localmente; no se suben `.exe` ni PPM. La entrada debe ser PPM binario P6. Podés convertir una foto con `magick foto.jpg -depth 8 entrada.ppm`. Para gráficos necesitás Python y `matplotlib`.

## Antes de la defensa

1. Leer el [informe](informe_final.pdf), la [guía de defensa](guia_defensa.md) y la [consigna](../consignas/CA-Practico%20Final-2026.pdf).
2. Poder explicar gris, promedio, Sobel, umbral, posterización y combinación; y el reparto con halos en MPI.
3. Identificar la diferencia entre procesos MPI, hilos OpenMP y la combinación híbrida.
4. Explicar `speedup = T_secuencial / T_paralelo` y `eficiencia = speedup / (procesos × hilos)`, así como que MPI incluye comunicación en el tiempo medido.
5. Mostrar una ejecución de `probar.sh`, los CSV y al menos los gráficos de tiempos y eficiencia. Verificar `git status` antes de introducir nuevos cambios.
