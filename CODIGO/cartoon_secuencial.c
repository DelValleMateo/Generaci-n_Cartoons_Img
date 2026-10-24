/*
 * Práctico Final Integrador - Computación Avanzada
 * Versión SECUENCIAL (C): genera una imagen estilo cartoon a partir de una imagen RGB.
 *
 * El cartoon sale de dos procesos independientes que después se suman:
 *   1) Detección de bordes: grises -> borroneo -> Sobel -> umbral
 *   2) Posterizado: reducir los niveles de cada canal (R, G, B)
 *
 * Compilar:  gcc -O2 -o cartoon_secuencial cartoon_secuencial.c
 * Usar:      ./cartoon_secuencial entrada.ppm salida.ppm [filtro] [niveles] [umbral] [bordes_oscuros]
 *              filtro          tamaño del borroneo: 3 o 5            (por defecto 3)
 *              niveles         niveles del posterizado: 3 o 9        (por defecto 3)
 *              umbral          a partir de qué magnitud hay borde    (por defecto 150)
 *              bordes_oscuros  0 = bordes se suman (blancos)         (por defecto 0)
 *                              1 = bordes se restan (contorno negro)
 *
 * Las imágenes se leen y se guardan en formato PPM (P6), que es el más simple:
 * una cabecera de texto y después los bytes R,G,B de cada pixel, fila por fila.
 */

#include <stdio.h>  /* fopen, fread, fwrite, fprintf, printf: archivos y pantalla */
#include <stdlib.h> /* malloc, calloc, free (memoria), atoi (texto a número), abs (valor absoluto) */
#include <time.h>   /* clock_gettime: para medir cuánto tarda el procesamiento */

/* Una imagen RGB.
 * Los pixels se guardan uno atrás del otro, fila por fila, con 3 bytes cada uno (R, G, B).
 * Para el pixel de la fila i y la columna j:
 *   - su byte rojo está en  datos[(i * ancho + j) * 3]
 *   - su byte verde en      datos[(i * ancho + j) * 3 + 1]
 *   - su byte azul en       datos[(i * ancho + j) * 3 + 2]
 * Cada byte es un unsigned char: un número entero de 0 a 255. */
typedef struct {
    int ancho;             /* cantidad de columnas */
    int alto;              /* cantidad de filas */
    unsigned char *datos;  /* puntero a los bytes de todos los pixels */
} Imagen;

/* ---------------------------------------------------------------------------
 * Entrada y salida de archivos PPM
 * ------------------------------------------------------------------------- */

/* Lee un número de la cabecera del PPM.
 * La cabecera es texto, por ejemplo:   P6 <salto> 800 800 <salto> 255 <salto>
 * y después vienen los bytes de la imagen. Esta función se saltea los espacios,
 * los saltos de línea y los comentarios (líneas que empiezan con '#') hasta
 * encontrar el número. Devuelve 1 si lo pudo leer y 0 si hubo un error. */
static int leerEnteroCabecera(FILE *f, int *valor) {
    int c = fgetc(f);  /* lee un carácter del archivo */

    /* mientras sea un espacio, un salto de línea, un tabulador o un '#', sigue de largo */
    while (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '#') {
        if (c == '#') {
            /* es un comentario: descarta todo hasta el final de la línea */
            while (c != '\n' && c != EOF) c = fgetc(f);
        }
        c = fgetc(f);  /* pasa al carácter siguiente */
    }

    if (c == EOF) return 0;  /* el archivo se terminó antes de encontrar el número */

    ungetc(c, f);  /* c es el primer dígito: se lo devuelve al archivo para que fscanf lea el número entero */
    return fscanf(f, "%d", valor) == 1;  /* fscanf devuelve cuántos valores leyó; 1 significa que salió bien */
}

/* Lee una imagen PPM (P6) desde un archivo y la guarda en *img.
 * Devuelve 1 si pudo leerla y 0 si no. */
int leerPPM(const char *ruta, Imagen *img) {
    FILE *f = fopen(ruta, "rb");  /* "rb" = abrir para leer en modo binario */
    if (f == NULL) return 0;      /* no existe o no se puede abrir */

    /* Los primeros 2 caracteres tienen que ser "P6": marca de PPM binario a color.
     * El arreglo tiene 3 lugares: 2 letras + el 0 final que usan las cadenas de C. */
    char magico[3] = {0};
    if (fscanf(f, "%2s", magico) != 1 || magico[0] != 'P' || magico[1] != '6') {
        fclose(f);
        return 0;
    }

    /* Después vienen tres números: ancho, alto y valor máximo de color (se asume 255).
     * El & le pasa a la función la dirección de la variable donde tiene que guardar el número.
     * img->ancho significa "el campo ancho de la imagen a la que apunta img". */
    int maximo;
    if (!leerEnteroCabecera(f, &img->ancho) || !leerEnteroCabecera(f, &img->alto) ||
        !leerEnteroCabecera(f, &maximo)) {
        fclose(f);
        return 0;
    }
    fgetc(f); /* después del 255 hay un único espacio en blanco antes de los datos: lo descarta */

    /* Cantidad de bytes de la imagen: ancho * alto * 3 canales.
     * El (size_t) hace que la cuenta se haga con un tipo grande y no se desborde con imágenes enormes. */
    size_t tam = (size_t)img->ancho * img->alto * 3;
    img->datos = (unsigned char *)malloc(tam);  /* pide esa cantidad de memoria */

    /* fread lee los 'tam' bytes de una vez y devuelve cuántos leyó.
     * Si malloc falló (NULL) o el archivo estaba incompleto, libera todo y avisa que hubo error. */
    if (img->datos == NULL || fread(img->datos, 1, tam, f) != tam) {
        free(img->datos);
        img->datos = NULL;
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

/* Guarda una imagen en un archivo PPM (P6).
 * Devuelve 1 si pudo guardarla y 0 si no. */
int guardarPPM(const char *ruta, const Imagen *img) {
    FILE *f = fopen(ruta, "wb");  /* "wb" = abrir para escribir en modo binario */
    if (f == NULL) return 0;

    size_t tam = (size_t)img->ancho * img->alto * 3;  /* bytes que ocupa la imagen */
    fprintf(f, "P6\n%d %d\n255\n", img->ancho, img->alto);  /* cabecera de texto */
    size_t escritos = fwrite(img->datos, 1, tam, f);        /* bytes de los pixels */
    fclose(f);
    return escritos == tam;  /* verdadero solo si se escribieron todos los bytes */
}

/* ---------------------------------------------------------------------------
 * Detección de bordes
 * ------------------------------------------------------------------------- */

/* Paso 0: pasar a escala de grises con la fórmula de la teoría (diapositiva 5).
 * Devuelve un arreglo con 1 byte por pixel. Quien la llama tiene que hacer free. */
unsigned char *aGrises(const Imagen *img) {
    /* un solo byte por pixel, porque un gris no necesita 3 canales */
    unsigned char *gris = (unsigned char *)malloc((size_t)img->ancho * img->alto);

    for (int i = 0; i < img->alto; i++) {        /* recorre las filas */
        for (int j = 0; j < img->ancho; j++) {   /* recorre las columnas de esa fila */
            int p = (i * img->ancho + j) * 3;    /* posición del byte rojo de este pixel */

            /* El ojo percibe más el verde que el rojo y el azul, por eso los pesos son distintos.
             * datos[p] es R, datos[p + 1] es G y datos[p + 2] es B. */
            double valor = 0.30 * img->datos[p] + 0.59 * img->datos[p + 1] + 0.11 * img->datos[p + 2];

            /* el gris se guarda en la posición i * ancho + j (sin el * 3, porque es 1 byte por pixel);
             * el cast a unsigned char descarta los decimales */
            gris[i * img->ancho + j] = (unsigned char)valor;
        }
    }
    return gris;
}

/* Paso 1 - Filtrado: borroneo para eliminar ruido.
 * Cada pixel pasa a ser el promedio de su vecindad de k x k.
 * En los límites de la imagen se promedian solo los vecinos que existen. */
unsigned char *borronear(const unsigned char *gris, int ancho, int alto, int k) {
    int radio = k / 2;  /* hasta dónde se mira alrededor del pixel: 3x3 -> radio 1, 5x5 -> radio 2 */
    unsigned char *salida = (unsigned char *)malloc((size_t)ancho * alto);

    for (int i = 0; i < alto; i++) {
        for (int j = 0; j < ancho; j++) {
            int suma = 0;      /* suma de los grises de los vecinos */
            int cantidad = 0;  /* cuántos vecinos se sumaron */

            /* di y dj son los desplazamientos respecto del pixel (i, j): de -radio a +radio */
            for (int di = -radio; di <= radio; di++) {
                for (int dj = -radio; dj <= radio; dj++) {
                    int ii = i + di;  /* fila del vecino */
                    int jj = j + dj;  /* columna del vecino */

                    /* solo se suma el vecino si está dentro de la imagen */
                    if (ii >= 0 && ii < alto && jj >= 0 && jj < ancho) {
                        suma += gris[ii * ancho + jj];
                        cantidad++;
                    }
                }
            }
            /* el pixel borroneado es el promedio (división entera) */
            salida[i * ancho + j] = suma / cantidad;
        }
    }
    return salida;
}

/* Pasos 2 y 3 - Resaltado y detección: operador de Sobel + umbral.
 * Sx detecta cambios horizontales y Sy cambios verticales. La magnitud |Sx| + |Sy| es alta
 * donde hay bordes. Si supera el umbral, el pixel es borde (255), si no es fondo (0).
 * Los pixels del marco de la imagen no tienen vecindad completa: quedan en 0 (negro, el valor neutro). */
unsigned char *detectarBordes(const unsigned char *borrosa, int ancho, int alto, int umbral) {
    /* calloc reserva memoria y la deja toda en 0: al principio ningún pixel es borde */
    unsigned char *bordes = (unsigned char *)calloc((size_t)ancho * alto, 1);

    /* se recorre solo el interior: de la fila 1 a la penúltima y de la columna 1 a la penúltima,
     * así cada pixel tiene sus 8 vecinos */
    for (int i = 1; i < alto - 1; i++) {
        for (int j = 1; j < ancho - 1; j++) {
            /* Los 8 vecinos del pixel (i, j). Las letras siguen la diapositiva 55 de la teoría:
             *     a b c
             *     d . f      (el pixel del centro no se usa en Sobel)
             *     g h k                                                        */
            int a = borrosa[(i - 1) * ancho + (j - 1)]; /* arriba izquierda */
            int b = borrosa[(i - 1) * ancho + j];       /* arriba */
            int c = borrosa[(i - 1) * ancho + (j + 1)]; /* arriba derecha */
            int d = borrosa[i * ancho + (j - 1)];       /* izquierda */
            int f = borrosa[i * ancho + (j + 1)];       /* derecha */
            int g = borrosa[(i + 1) * ancho + (j - 1)]; /* abajo izquierda */
            int h = borrosa[(i + 1) * ancho + j];       /* abajo */
            int k = borrosa[(i + 1) * ancho + (j + 1)]; /* abajo derecha */

            /* Máscaras de Sobel
             *   Sx = -1 0 1     Sy = -1 -2 -1
             *        -2 0 2           0  0  0
             *        -1 0 1           1  2  1
             * sx = columna derecha - columna izquierda (el pixel del medio pesa el doble):
             *      detecta cambios de izquierda a derecha, o sea bordes verticales.
             * sy = fila de abajo - fila de arriba: detecta bordes horizontales.
             * En una zona pareja las restas dan casi 0; donde hay un salto de brillo dan un número grande. */
            int sx = (c + 2 * f + k) - (a + 2 * d + g);
            int sy = (g + 2 * h + k) - (a + 2 * b + c);

            /* la magnitud suma la intensidad en las dos direcciones (con abs porque puede dar negativo) */
            int magnitud = abs(sx) + abs(sy);

            /* si supera el umbral es borde: se marca con 255 (blanco) */
            if (magnitud > umbral) bordes[i * ancho + j] = 255;
        }
    }
    return bordes;
}

/* ---------------------------------------------------------------------------
 * Posterizado
 * ------------------------------------------------------------------------- */

/* Divide [0, 255] en 'niveles' intervalos iguales y reemplaza cada valor por el punto medio
 * de su intervalo. Se aplica a cada canal (R, G y B) por separado.
 * Ejemplo con 3 niveles: [0..85] -> 42, [86..170] -> 128, [171..255] -> 213. */
Imagen posterizar(const Imagen *img, int niveles) {
    /* se crea una imagen nueva del mismo tamaño, así la original no se modifica */
    Imagen salida;
    salida.ancho = img->ancho;
    salida.alto = img->alto;
    size_t tam = (size_t)img->ancho * img->alto * 3;
    salida.datos = (unsigned char *)malloc(tam);

    /* Se recorren todos los bytes de la imagen. No hace falta separar R, G y B porque
     * a los tres canales se les aplica exactamente la misma operación. */
    for (size_t p = 0; p < tam; p++) {
        /* a qué intervalo pertenece el valor (división entera): con 3 niveles da 0, 1 o 2 */
        int intervalo = img->datos[p] * niveles / 256;

        /* valor representativo: el punto medio del intervalo.
         * Dos colores del mismo intervalo quedan iguales, y por eso la imagen se ve en zonas planas. */
        salida.datos[p] = (unsigned char)((intervalo + 0.5) * 256.0 / niveles);
    }
    return salida;
}

/* ---------------------------------------------------------------------------
 * Combinación final
 * ------------------------------------------------------------------------- */

/* Suma la imagen posterizada con la imagen de bordes (como pide el enunciado).
 * Si bordesOscuros es 1, en lugar de sumar los bordes se restan, y el contorno queda negro.
 * El resultado se recorta al rango [0, 255]. */
Imagen combinar(const Imagen *posterizada, const unsigned char *bordes, int bordesOscuros) {
    Imagen salida;
    salida.ancho = posterizada->ancho;
    salida.alto = posterizada->alto;
    salida.datos = (unsigned char *)malloc((size_t)salida.ancho * salida.alto * 3);

    for (int i = 0; i < posterizada->alto; i++) {
        for (int j = 0; j < posterizada->ancho; j++) {
            int borde = bordes[i * posterizada->ancho + j];  /* 0 (fondo) o 255 (borde) */

            for (int c = 0; c < 3; c++) {  /* los tres canales: R, G y B */
                int p = (i * posterizada->ancho + j) * 3 + c;

                /* condición ? si_se_cumple : si_no
                 * Con bordesOscuros se resta el borde; si no, se suma. */
                int valor = bordesOscuros ? posterizada->datos[p] - borde : posterizada->datos[p] + borde;

                /* recorte: la suma puede pasarse de 255 y la resta puede dar negativo */
                if (valor < 0) valor = 0;
                if (valor > 255) valor = 255;
                salida.datos[p] = (unsigned char)valor;
            }
        }
    }
    return salida;
}

/* ---------------------------------------------------------------------------
 * Programa principal
 * ------------------------------------------------------------------------- */

/* Devuelve el tiempo actual en segundos, para medir cuánto tarda el procesamiento.
 * static significa que la función solo se ve dentro de este archivo.
 * CLOCK_MONOTONIC es un reloj que no se altera si cambian la hora del sistema. */
static double ahora(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;  /* segundos + nanosegundos pasados a segundos */
}

/* argc es la cantidad de argumentos y argv son los argumentos como texto.
 * argv[0] es el nombre del programa, argv[1] la imagen de entrada y argv[2] la de salida. */
int main(int argc, char **argv) {
    /* si faltan la entrada o la salida, se muestra cómo se usa y se termina con error (1) */
    if (argc < 3) {
        fprintf(stderr, "Uso: %s entrada.ppm salida.ppm [filtro=3] [niveles=3] [umbral=150] [bordes_oscuros=0]\n", argv[0]);
        return 1;
    }
    const char *rutaEntrada = argv[1];
    const char *rutaSalida = argv[2];

    /* Parámetros opcionales: si el usuario los escribió, atoi convierte el texto a número;
     * si no, se usa el valor por defecto. */
    int filtro = (argc > 3) ? atoi(argv[3]) : 3;
    int niveles = (argc > 4) ? atoi(argv[4]) : 3;
    int umbral = (argc > 5) ? atoi(argv[5]) : 150;
    int bordesOscuros = (argc > 6) ? (atoi(argv[6]) != 0) : 0;

    /* se carga la imagen de entrada */
    Imagen original;
    if (!leerPPM(rutaEntrada, &original)) {
        fprintf(stderr, "No se pudo leer la imagen (tiene que ser un PPM tipo P6): %s\n", rutaEntrada);
        return 1;
    }

    /* Se mide solo el procesamiento, sin leer ni guardar el archivo.
     * Así más adelante se pueden comparar las versiones paralelas de forma justa. */
    double inicio = ahora();

    /* 1) Bordes: grises -> borroneo -> Sobel con umbral */
    unsigned char *gris = aGrises(&original);
    unsigned char *borrosa = borronear(gris, original.ancho, original.alto, filtro);
    unsigned char *bordes = detectarBordes(borrosa, original.ancho, original.alto, umbral);

    /* 2) Posterizado (sobre la imagen original en color; no depende de los bordes) */
    Imagen posterizada = posterizar(&original, niveles);

    /* 3) Suma de ambos resultados */
    Imagen cartoon = combinar(&posterizada, bordes, bordesOscuros);

    double segundos = ahora() - inicio;  /* tiempo que tardó el procesamiento */

    /* se guarda la imagen resultante */
    if (!guardarPPM(rutaSalida, &cartoon)) {
        fprintf(stderr, "No se pudo guardar la imagen: %s\n", rutaSalida);
        return 1;
    }

    /* se muestran los parámetros usados y el tiempo (%d = entero, %.4f = decimal con 4 dígitos) */
    printf("Imagen %dx%d | filtro %dx%d | niveles %d | umbral %d | tiempo %.4f s\n",
           original.ancho, original.alto, filtro, filtro, niveles, umbral, segundos);

    /* Se libera toda la memoria que se pidió con malloc/calloc: cada reserva tiene su free */
    free(original.datos);
    free(gris);
    free(borrosa);
    free(bordes);
    free(posterizada.datos);
    free(cartoon.datos);
    return 0;  /* 0 = el programa terminó bien */
}
