# Prompt para la IA de Mateo

Copiá el texto siguiente en ChatGPT o Codex junto con acceso a tu copia local del repositorio.

---

Soy Mateo del Valle. Junto con Luca Saboredo desarrollé el Práctico Final Integrador de Computación Avanzada de UADER: generación de imágenes cartoon RGB a partir de archivos PPM P6. Repositorio compartido: https://github.com/DelValleMateo/Generaci-n_Cartoons_Img (rama `main`).

Antes de modificar cualquier archivo, comprobá `git status`, actualizá de forma segura desde `origin/main` y leé el README principal, la consigna en `consignas/CA-Practico Final-2026.pdf`, el código de `CODIGO/` y la documentación de `documentacion/`. Si tengo modificaciones locales o la rama diverge, explicame cómo conservarlas e integrarlas; no sobrescribas el trabajo de Luca ni fuerces un push.

La estructura actual es: `CODIGO/` para cuatro programas C++ y `probar.sh`; `scripts/` para benchmark, pruebas, validación, gráficos e informe; `ejemplos/` para foto y cuatro PNG; `resultados/matriz_completa/` para CSV y validación; `resultados/registros/` para logs y piloto; `graficos/` para seis figuras; `documentacion/` para informe y guías; y `consignas/` para el enunciado y apuntes.

Los cuatro programas son `punto1_secuencial.cpp` (referencia), `punto2_mpi.cpp` (filas distribuidas con halos), `punto3_openmp.cpp` (bucles paralelos) y `punto4_hibrido.cpp` (MPI + OpenMP). El algoritmo pasa RGB a gris, borrona, detecta bordes con Sobel y umbral, posteriza el color y combina ambas ramas. Se mantuvieron las fórmulas originales y se reforzaron las validaciones y el manejo de errores.

El entorno comprobado fue Windows con MSYS2 UCRT64, GCC 16.2, OpenMP y Microsoft MPI 10.1.3. No presupongas que OpenMPI o WSL fueron usados para producir los resultados. `CODIGO/README.md` contiene además instrucciones históricas para Linux/WSL; el README de la raíz es la guía principal del estado entregado.

Ya se hicieron 396 mediciones reales (12 cargas × 11 configuraciones × 3 repeticiones), 40 comparaciones byte a byte, 260 comparaciones de píxeles con un oráculo independiente y 32 pruebas de errores. Los datos están en `resultados/matriz_completa/mediciones.csv` y `resumen.csv`; hay seis gráficos en `graficos/`, una foto y cuatro salidas en `ejemplos/`, y un informe final PDF/DOCX con guía de defensa en `documentacion/`. Leé `documentacion/GUIA_PARA_MATEO.md` para entender lo realizado y los límites de la comparación de tiempos.

Continuá desde el estado más reciente de `main`. No repitas las 396 mediciones ni reemplaces CSV, gráficos o el informe sin una necesidad concreta y verificable. Para comprobar cambios, compilá las cuatro versiones, ejecutá `bash CODIGO/probar.sh`, `python scripts/test_casos_limite.py` y `python scripts/validar_resultados.py`. Si modificás código o rutas, justificá el cambio y verificá equivalencia. Mantené los ejecutables, PPM grandes y temporales fuera de Git. Antes de publicar, revisá el diff y coordiná para no perder cambios de ninguno de los dos.
