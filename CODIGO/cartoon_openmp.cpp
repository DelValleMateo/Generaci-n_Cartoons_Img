// Práctico Final Integrador - Computación Avanzada
// Versión OPENMP (C++): genera una imagen estilo cartoon = bordes + posterizado.
//
// Compilar:  g++ -O2 -fopenmp -o cartoon_openmp cartoon_openmp.cpp
// Usar:      ./cartoon_openmp entrada.ppm salida.ppm [filtro=3] [niveles=3] [umbral=150] [bordes_oscuros=0] [hilos=4]
//            bordes_oscuros: 0 = los bordes se suman (blancos), 1 = se restan (contorno negro)
// Las imágenes son PPM (P6): una cabecera de texto y después los bytes R,G,B de cada pixel, fila por fila.

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
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
    f.get();  // el espacio en blanco antes de los datos

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

// Cada 'parallel for' reparte las filas (o pixeles) entre los hilos; al terminar el for hay una barrera
// implícita, así cada etapa empieza recién cuando la anterior terminó en todos los hilos.

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
    if (argc < 3) {
        cerr << "Uso: " << argv[0] << " entrada.ppm salida.ppm [filtro=3] [niveles=3] [umbral=150] [bordes_oscuros=0] [hilos=4]\n";
        return 1;
    }
    int filtro = (argc > 3) ? atoi(argv[3]) : 3;
    int niveles = (argc > 4) ? atoi(argv[4]) : 3;
    int umbral = (argc > 5) ? atoi(argv[5]) : 150;
    bool bordesOscuros = (argc > 6) && atoi(argv[6]) != 0;
    int hilos = (argc > 7) ? atoi(argv[7]) : 4;
    omp_set_num_threads(hilos);

    Imagen original;
    if (!leerPPM(argv[1], original)) {
        cerr << "No se pudo leer la imagen (tiene que ser un PPM tipo P6): " << argv[1] << "\n";
        return 1;
    }

    // Se mide solo el procesamiento, sin leer ni guardar el archivo.
    double inicio = omp_get_wtime();

    auto gris = aGrises(original);                                                // 1) bordes
    auto borrosa = borronear(gris, original.ancho, original.alto, filtro);
    auto bordes = detectarBordes(borrosa, original.ancho, original.alto, umbral);
    auto posterizada = posterizar(original, niveles);                             // 2) posterizado
    auto cartoon = combinar(posterizada, bordes, bordesOscuros);                  // 3) suma de ambos

    double segundos = omp_get_wtime() - inicio;

    if (!guardarPPM(argv[2], cartoon)) {
        cerr << "No se pudo guardar la imagen: " << argv[2] << "\n";
        return 1;
    }
    cout << "Imagen " << original.ancho << "x" << original.alto << " | filtro " << filtro << "x" << filtro
         << " | niveles " << niveles << " | umbral " << umbral << " | hilos " << hilos << " | tiempo " << segundos << " s\n";
    return 0;
}
