// Práctico Final Integrador - Computación Avanzada
// Versión HÍBRIDA MPI + OPENMP (C++): genera una imagen estilo cartoon = bordes + posterizado.
//
// Compilar:  mpicxx -O2 -fopenmp -o punto4_hibrido punto4_hibrido.cpp
// Usar:      mpirun -np PROCESOS ./punto4_hibrido entrada.ppm salida.ppm [filtro=3] [niveles=3] [umbral=150] [bordes_oscuros=0] [hilos=2]
//            bordes_oscuros: 0 = los bordes se suman (blancos), 1 = se restan (contorno negro)
// Las imágenes son PPM (P6): una cabecera de texto y después los bytes R,G,B de cada pixel, fila por fila.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <omp.h>
#include <string>
#include <vector>
using namespace std;

// El pixel (i, j) ocupa 3 bytes seguidos desde la posición (i * ancho + j) * 3.
struct Imagen {
    int ancho = 0, alto = 0;
    vector<unsigned char> datos;
};

bool leerPPM(const string& ruta, Imagen& img) {
    ifstream f(ruta, ios::binary);
    string magico;
    int maximo;
    f >> magico;
    if (!f || magico != "P6") return false;

    // lee un número de la cabecera, salteando las líneas de comentario que empiezan con '#'
    auto leerNumero = [&](int& x) {
        while ((f >> ws).peek() == '#') f.ignore(1000, '\n');
        f >> x;
    };
    leerNumero(img.ancho);
    leerNumero(img.alto);
    leerNumero(maximo);
    if (!f || img.ancho <= 0 || img.alto <= 0 || maximo != 255 ||
        (size_t)img.ancho * img.alto > (size_t)2147483647 / 3) return false;
    int separador = f.get();
    if (separador == EOF || !isspace((unsigned char)separador)) return false;
    if (separador == '\r' && f.peek() == '\n') f.get();

    img.datos.resize((size_t)img.ancho * img.alto * 3);
    f.read((char*)img.datos.data(), img.datos.size());
    return (bool)f;
}

bool guardarPPM(const string& ruta, const Imagen& img) {
    ofstream f(ruta, ios::binary);
    f << "P6\n" << img.ancho << " " << img.alto << "\n255\n";
    f.write((const char*)img.datos.data(), img.datos.size());
    return (bool)f;
}

// Estas funciones son las mismas de la versión OpenMP, pero cada proceso MPI las aplica solo a su bloque de filas.
// Dentro de cada proceso, 'parallel for' reparte esas filas entre los hilos.

// Pasa a escala de grises (1 byte por pixel) con la fórmula de la teoría.
vector<unsigned char> aGrises(const Imagen& img) {
    vector<unsigned char> gris((size_t)img.ancho * img.alto);
    #pragma omp parallel for
    for (size_t p = 0; p < gris.size(); p++)
        gris[p] = 0.30 * img.datos[3 * p] + 0.59 * img.datos[3 * p + 1] + 0.11 * img.datos[3 * p + 2];
    return gris;
}

// Filtrado: cada pixel pasa a ser el promedio de su vecindad de k x k.
// En los límites de la imagen se promedian solo los vecinos que existen.
vector<unsigned char> borronear(const vector<unsigned char>& gris, int ancho, int alto, int k) {
    int radio = k / 2;  // 3x3 -> radio 1, 5x5 -> radio 2
    vector<unsigned char> salida(gris.size());
    #pragma omp parallel for
    for (int i = 0; i < alto; i++) {
        for (int j = 0; j < ancho; j++) {
            int suma = 0, cantidad = 0;
            for (int di = -radio; di <= radio; di++) {
                for (int dj = -radio; dj <= radio; dj++) {
                    int ii = i + di, jj = j + dj;
                    if (ii >= 0 && ii < alto && jj >= 0 && jj < ancho) {
                        suma += gris[ii * ancho + jj];
                        cantidad++;
                    }
                }
            }
            salida[i * ancho + j] = suma / cantidad;
        }
    }
    return salida;
}

// Resaltado y detección: Sobel + umbral. Devuelve 255 en los bordes y 0 en el resto.
// Sx detecta cambios horizontales y Sy verticales; |Sx| + |Sy| es grande donde hay un borde.
// El marco de la imagen no tiene vecindad completa y queda en 0 (negro, el valor neutro).
vector<unsigned char> detectarBordes(const vector<unsigned char>& borrosa, int ancho, int alto, int umbral) {
    vector<unsigned char> bordes(borrosa.size(), 0);
    #pragma omp parallel for
    for (int i = 1; i < alto - 1; i++) {
        for (int j = 1; j < ancho - 1; j++) {
            // v(di, dj) es el vecino que está di filas y dj columnas desde el pixel (i, j)
            auto v = [&](int di, int dj) { return (int)borrosa[(i + di) * ancho + (j + dj)]; };

            int sx = (v(-1, 1) + 2 * v(0, 1) + v(1, 1)) - (v(-1, -1) + 2 * v(0, -1) + v(1, -1));  // derecha - izquierda
            int sy = (v(1, -1) + 2 * v(1, 0) + v(1, 1)) - (v(-1, -1) + 2 * v(-1, 0) + v(-1, 1));  // abajo - arriba

            if (abs(sx) + abs(sy) > umbral) bordes[i * ancho + j] = 255;
        }
    }
    return bordes;
}

// Divide [0, 255] en 'niveles' intervalos y reemplaza cada valor por el punto medio de su intervalo.
// Se aplica igual a los tres canales. Con 3 niveles: [0..85] -> 42, [86..170] -> 128, [171..255] -> 213.
// 'img' se recibe por copia, así la original no se modifica.
Imagen posterizar(Imagen img, int niveles) {
    #pragma omp parallel for
    for (size_t p = 0; p < img.datos.size(); p++)
        img.datos[p] = (img.datos[p] * niveles / 256 + 0.5) * 256.0 / niveles;
    return img;
}

// Suma la imagen posterizada con la de bordes (o la resta, si bordesOscuros) y recorta a [0, 255].
Imagen combinar(Imagen img, const vector<unsigned char>& bordes, bool bordesOscuros) {
    #pragma omp parallel for
    for (size_t p = 0; p < bordes.size(); p++) {
        for (int c = 0; c < 3; c++) {
            int valor = img.datos[3 * p + c] + (bordesOscuros ? -bordes[p] : bordes[p]);
            img.datos[3 * p + c] = max(0, min(255, valor));
        }
    }
    return img;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, procesos;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &procesos);

    if (argc < 3) {
        if (rank == 0) cerr << "Uso: mpirun -np P " << argv[0] << " entrada.ppm salida.ppm [filtro=3] [niveles=3] [umbral=150] [bordes_oscuros=0] [hilos=2]\n";
        MPI_Finalize();
        return 1;
    }
    int filtro = (argc > 3) ? atoi(argv[3]) : 3;
    int niveles = (argc > 4) ? atoi(argv[4]) : 3;
    int umbral = (argc > 5) ? atoi(argv[5]) : 150;
    bool bordesOscuros = (argc > 6) && atoi(argv[6]) != 0;
    int hilos = (argc > 7) ? atoi(argv[7]) : 2;
    if ((filtro != 3 && filtro != 5) || (niveles != 3 && niveles != 9) || umbral < 0 || umbral > 2040 || hilos < 1) {
        if (rank == 0) cerr << "Parametros invalidos: filtro 3/5, niveles 3/9, umbral 0..2040, hilos >= 1\n";
        MPI_Finalize();
        return 1;
    }
    omp_set_dynamic(0);
    omp_set_num_threads(hilos);

    // 1) El proceso 0 lee la imagen y avisa el tamaño a todos (si falla avisa tamaño 0).
    Imagen original;
    int medidas[2] = {0, 0};
    if (rank == 0 && leerPPM(argv[1], original)) {
        medidas[0] = original.ancho;
        medidas[1] = original.alto;
    }
    MPI_Bcast(medidas, 2, MPI_INT, 0, MPI_COMM_WORLD);
    int ancho = medidas[0], alto = medidas[1];
    if (ancho == 0) {
        if (rank == 0) cerr << "No se pudo leer la imagen (tiene que ser un PPM tipo P6): " << argv[1] << "\n";
        MPI_Finalize();
        return 1;
    }

    // 2) Reparto de filas: a cada proceso le toca un bloque de filas consecutivas ("filas propias").
    //    Para calcular bien las filas de los extremos del bloque necesita también filas de los bloques vecinos (halo):
    //    el filtro de radio k/2 y Sobel (radio 1) -> halo = k/2 + 1 filas de cada lado (3x3: 2, 5x5: 3).
    int halo = filtro / 2 + 1;
    vector<int> primera(procesos), cantidad(procesos);   // filas propias de cada proceso
    vector<int> bytesEnvio(procesos), desde(procesos);   // lo que se le manda: filas propias + halo
    vector<int> bytesPropios(procesos), desdePropio(procesos);  // lo que devuelve: solo filas propias
    int base = alto / procesos, resto = alto % procesos;
    for (int r = 0, fila = 0; r < procesos; r++) {
        primera[r] = fila;
        cantidad[r] = base + (r < resto ? 1 : 0);
        fila += cantidad[r];

        int ini = max(0, primera[r] - halo);
        int fin = min(alto, primera[r] + cantidad[r] + halo);
        bytesEnvio[r] = (cantidad[r] > 0) ? (fin - ini) * ancho * 3 : 0;
        desde[r] = ini * ancho * 3;
        bytesPropios[r] = cantidad[r] * ancho * 3;
        desdePropio[r] = primera[r] * ancho * 3;
    }

    // Se mide desde que el reparto empieza hasta que el proceso 0 tiene todo el resultado (incluye la comunicación).
    MPI_Barrier(MPI_COMM_WORLD);
    double inicio = MPI_Wtime();

    // 3) Cada proceso recibe su bloque con halo.
    Imagen tira;
    tira.ancho = ancho;
    tira.datos.resize(bytesEnvio[rank]);
    tira.alto = tira.datos.size() / (ancho * 3);
    MPI_Scatterv(original.datos.data(), bytesEnvio.data(), desde.data(), MPI_UNSIGNED_CHAR,
                 tira.datos.data(), bytesEnvio[rank], MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    // 4) Cada proceso hace el cartoon de su bloque. Sobre la tira se aplican las mismas funciones que en el secuencial.
    //    El borde de la tira (si no es borde de la imagen) queda con valores incompletos, pero justo
    //    esas filas son el halo y se descartan: solo se conservan las filas propias.
    Imagen propia;
    propia.ancho = ancho;
    propia.alto = cantidad[rank];
    if (cantidad[rank] > 0) {
        int ini = max(0, primera[rank] - halo);
        int saltar = primera[rank] - ini;  // filas de halo de arriba

        auto gris = aGrises(tira);
        auto borrosa = borronear(gris, ancho, tira.alto, filtro);
        auto bordes = detectarBordes(borrosa, ancho, tira.alto, umbral);

        propia.datos.assign(tira.datos.begin() + (size_t)saltar * ancho * 3,
                            tira.datos.begin() + (size_t)(saltar + propia.alto) * ancho * 3);
        vector<unsigned char> bordesPropios(bordes.begin() + (size_t)saltar * ancho,
                                            bordes.begin() + (size_t)(saltar + propia.alto) * ancho);
        propia = combinar(posterizar(propia, niveles), bordesPropios, bordesOscuros);
    }

    // 5) El proceso 0 junta los bloques en la imagen final.
    Imagen cartoon;
    cartoon.ancho = ancho;
    cartoon.alto = alto;
    if (rank == 0) cartoon.datos.resize((size_t)ancho * alto * 3);
    MPI_Gatherv(propia.datos.data(), bytesPropios[rank], MPI_UNSIGNED_CHAR,
                cartoon.datos.data(), bytesPropios.data(), desdePropio.data(), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    double segundos = MPI_Wtime() - inicio;

    int hilosEfectivos = 0, minimoHilos = 0, maximoHilos = 0;
    #pragma omp parallel
    {
        #pragma omp single
        hilosEfectivos = omp_get_num_threads();
    }
    MPI_Reduce(&hilosEfectivos, &minimoHilos, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&hilosEfectivos, &maximoHilos, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);

    int errorSalida = 0;
    if (rank == 0) {
        if (!guardarPPM(argv[2], cartoon)) {
            cerr << "No se pudo guardar la imagen: " << argv[2] << "\n";
            errorSalida = 1;
        }
        cout << "Imagen " << ancho << "x" << alto << " | filtro " << filtro << "x" << filtro
             << " | niveles " << niveles << " | umbral " << umbral << " | procesos " << procesos
             << " | hilos " << hilos << " | hilos efectivos " << minimoHilos << ".." << maximoHilos
             << " | tiempo " << segundos << " s\n";
    }
    MPI_Finalize();
    return errorSalida;
}
