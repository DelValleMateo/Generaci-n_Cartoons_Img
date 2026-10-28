# Guía breve para la defensa del práctico

## La idea en un minuto

Partimos de un PPM P6 RGB. Para encontrar contornos, convertimos a gris con `0,30R + 0,59G + 0,11B`, promediamos una vecindad de 3 × 3 o 5 × 5, aplicamos Sobel y dejamos blancos los puntos cuya magnitud `|Sx| + |Sy|` supera 150. En otra rama, cada canal RGB se reduce a 3 o 9 niveles. Sumamos los bordes a la imagen posterizada y limitamos cada canal a 255. La opción de bordes oscuros invierte esa combinación. La consigna admite el formato elegido porque los píxeles quedan disponibles como matriz RGB.

## Qué hace cada versión

| Versión | Reparto del trabajo | Comunicación o sincronización |
|---|---|---|
| Secuencial | Un proceso recorre toda la imagen | No hay paralelismo |
| MPI | Cada proceso recibe filas contiguas más halos | `MPI_Bcast`, `MPI_Scatterv`, `MPI_Gatherv` |
| OpenMP | Varios hilos reparten iteraciones de cada etapa | Barrera implícita al final de `parallel for` |
| Híbrida | MPI reparte bloques y OpenMP reparte píxeles dentro de cada bloque | Colectivas MPI fuera de las regiones OpenMP |

La altura no siempre es divisible por la cantidad de procesos. Los primeros procesos reciben una fila propia adicional cuando hay resto. `Scatterv` permite tamaños distintos y `Gatherv` reconstruye la imagen final en orden. Para filtro 3 × 3 se necesitan 2 filas de halo por lado: una para el promedio y otra para Sobel. Para 5 × 5 se necesitan 3. Solo se devuelve el bloque de filas propias. Esto evita líneas espurias en los límites entre procesos.

En OpenMP, cada iteración escribe un píxel o una fila diferente. Los acumuladores del filtro viven dentro de la iteración, así que no se comparten. Una etapa termina antes de que empiece la siguiente. Se deshabilitó el ajuste dinámico del número de hilos y el programa informa cuántos actuaron efectivamente. En la versión híbrida se reduce entre procesos el mínimo y máximo de hilos observados para comprobar la configuración.

## Instrucciones que conviene poder explicar

- `MPI_Init` y `MPI_Finalize`: abren y cierran el entorno MPI.
- `MPI_Comm_rank` y `MPI_Comm_size`: identifican el proceso y cuentan los procesos del comunicador.
- `MPI_Bcast`: comunica ancho y alto desde el proceso raíz.
- `MPI_Scatterv`: envía bloques de longitudes diferentes, incluidos los halos.
- `MPI_Gatherv`: reúne solo las filas propias en el orden correcto.
- `MPI_Barrier`: sincroniza el inicio de la medición MPI.
- `MPI_Wtime`: devuelve tiempo de pared para la región MPI medida.
- `#pragma omp parallel for`: reparte las iteraciones entre hilos.
- `omp_set_num_threads`, `omp_set_dynamic(0)` y `omp_get_num_threads`: fijan la cantidad solicitada y comprueban la efectiva.
- `omp_get_wtime`: mide tiempo de pared en OpenMP.

## Qué se probó realmente

Se midieron 800 × 800, 2000 × 2000 y 5000 × 5000; filtro 3 × 3 y 5 × 5; posterización de 3 y 9 niveles. Son **12 casos**. Cada uno se ejecutó con 11 configuraciones: secuencial; OpenMP 1, 2, 4 y 8 hilos; MPI 1, 2 y 4 procesos; híbrida 2 × 2, 2 × 4 y 4 × 2. Cada configuración tiene **3 repeticiones**, por lo que hay **396 tiempos individuales** y **132 promedios**. El validador comprobó que cada caso tiene tres repeticiones únicas, que las fórmulas son correctas y que todas las salidas paralelas coinciden por SHA-256 con la secuencial.

`probar.sh` terminó con `TODO OK` en 40 comparaciones de 800 × 800. Además hubo 260 comparaciones píxel por píxel con una referencia independiente sobre imágenes pequeñas, incluso con más procesos que filas, y 32 pruebas de entrada o salida inválida que terminaron con error. Una fotografía real de 1330 × 1002 se convirtió a cartoon con las cuatro versiones; los cuatro PNG resultaron idénticos.

## Cómo leer Speedup y eficiencia

Para un mismo tamaño, filtro y número de niveles: `Speedup = tiempo secuencial promedio / tiempo paralelo promedio`. `Eficiencia = Speedup / unidades de ejecución`. Las unidades son hilos para OpenMP, procesos para MPI y procesos × hilos para la híbrida.

Ejemplo real de **5000 × 5000, filtro 5 × 5, 9 niveles**: secuencial `0,665927 s`; OpenMP 8 hilos `0,175667 s`; MPI 4 procesos `0,262911 s`; híbrida 4 × 2 `0,172924 s`. Los speedups respectivos son `3,79`, `2,53` y `3,85`. La eficiencia de la híbrida es `3,85 / 8 = 0,48`. Las cifras exactas y cada repetición están en `resultados/matriz_completa/`.

OpenMP 8 hilos fue la configuración más rápida en 8 de los 12 casos; híbrida 4 × 2 ganó los otros 4. El speedup medio de OpenMP 8 hilos sobre las cuatro combinaciones de filtro y niveles creció de `2,38` en 800 × 800 a `3,51` en 5000 × 5000. Con más píxeles hay más trabajo para amortizar la sobrecarga. El crecimiento no es lineal: memoria, hilos, copias y comunicación cuestan tiempo. MPI de un solo proceso y OpenMP de un hilo fueron más lentos que el programa secuencial porque ejecutan trabajo adicional sin aprovechar concurrencia.

**Límite de la comparación:** los cronómetros excluyen leer y guardar archivos. MPI e híbrida incluyen `Scatterv` y `Gatherv`; secuencial y OpenMP miden solo procesamiento. El experimento no separa cuánto tiempo corresponde exactamente a cada comunicación. Se ejecutó en una sola computadora Windows con AMD Ryzen 7 5700 y 16 procesadores lógicos, no en un clúster.

## Preguntas posibles de Fabiana

**¿Por qué el halo es mayor que el radio del filtro?** Sobel lee una fila vecina de la imagen borrosa. Para calcular correctamente esa fila borrosa se necesita una fila adicional de la imagen gris: `k/2 + 1`.

**¿Por qué `Scatterv` en lugar de `Scatter`?** La cantidad de filas propias puede variar por el resto de la división, y los extremos físicos de la imagen tienen halos recortados. Los bloques enviados no tienen necesariamente la misma longitud.

**¿Se comunican los procesos durante Sobel?** No. Cada proceso recibe desde el inicio todas las filas de halo necesarias. Calcula su bloque de forma independiente y devuelve solo filas propias.

**¿Cómo saben que no hay una costura incorrecta entre bloques MPI?** Los 396 resultados de benchmark se agruparon por los doce casos; dentro de cada caso hubo un único SHA-256. También se compararon imágenes pequeñas byte por byte con una referencia independiente, con divisiones no exactas y más procesos que filas.

**¿Hay una condición de carrera en el borroneo OpenMP?** Cada iteración usa su propia `suma` y `cantidad` y escribe un elemento exclusivo de `salida`. Las demás etapas siguen el mismo patrón de escritura independiente.

**¿La versión híbrida llama a MPI desde varios hilos?** No. MPI reparte y reúne los datos fuera de las regiones OpenMP. Los hilos se usan dentro del procesamiento de cada bloque.

**¿Cómo verificaron que los hilos realmente se iniciaron?** El programa registra `omp_get_num_threads()` en una región paralela y la prueba de casos límite comprueba el valor. En híbrido se compara el mínimo y máximo entre procesos.

**¿Por qué 8 hilos no dan speedup 8?** Hay secciones no paralelas, barreras entre etapas, acceso a memoria compartida y costos de iniciar las regiones paralelas. Además el tiempo secuencial se mide en un programa distinto, optimizado para una sola unidad.

**¿Por qué MPI con un proceso puede ser más lento?** Todavía hace reparto, recolección y copias de bloques. No gana cómputo concurrente con una sola unidad.

**¿Pueden atribuir una diferencia de tiempo exclusivamente a la comunicación MPI?** No. El cronómetro MPI la incluye, pero también hay otras diferencias de memoria y ejecución. Para cuantificarla habría que instrumentar Scatterv y Gatherv por separado.

**¿Cuándo convendría OpenMP y cuándo MPI?** En esta máquina compartida OpenMP fue normalmente más rápido y evita mover la imagen entre procesos. MPI es necesario cuando la memoria está distribuida entre nodos. Un ensayo en clúster podría dar otra relación de costos.

**¿Qué aporta el híbrido?** Combina procesos con hilos dentro de cada proceso. La configuración 4 × 2 ganó cuatro casos, pero su eficiencia fue menor que la ideal y no permite afirmar que siempre convenga.

**¿Cómo afecta 5 × 5?** Visita más vecinos por píxel que 3 × 3. En la secuencial, la relación media tiempo 5 × 5 / tiempo 3 × 3 fue 1,45 en 800 × 800, 1,59 en 2000 × 2000 y 1,61 en 5000 × 5000.

**¿Qué harían para mejorar el experimento?** Aumentar las repeticiones, controlar la carga del equipo, medir por separado cómputo y comunicación, y repetir en varios nodos. Eso complementaría los datos actuales; no cambia las cifras registradas.
