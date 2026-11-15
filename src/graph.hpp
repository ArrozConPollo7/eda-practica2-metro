// graph.hpp - estructura del sistema de transporte y del grafo.
#pragma once

#include <string>
#include <vector>

// Una estación del sistema (una fila de data/stations.csv).
struct Estacion {
    std::string id;            // identificador corto: "san_antonio"
    std::string nombre;        // nombre para mostrar: "San Antonio"
    double lat = 0.0;
    double lon = 0.0;
    std::vector<int> lineas;   // índices dentro de Graph::lineas
};

// Línea del sistema, con el color que usa el SVG.
struct Linea {
    std::string codigo;        // "A", "B", "K", "J", "L", "H", "M", "T-A"
    std::string color;         // "#0065B3"
};

// Nodo del grafo expandido: el par (estación, línea). Modelar el par y no la
// estación sola es lo que permite cobrar el transbordo: cambiar de línea
// dentro de una estación es una arista más del grafo.
struct Nodo {
    int estacion = -1;
    int linea = -1;
};

// Arista de una lista de adyacencia; el costo está en minutos.
struct Arista {
    int destino = -1;
    double minutos = 0.0;
    bool transbordo = false;
};

// Grafo dirigido guardado como listas de adyacencia (un vector de vectores).
// Ocupa O(V + E) en memoria y recorrer los vecinos de un nodo cuesta
// O(grado del nodo), que es la representación adecuada para Dijkstra y A*.
class Graph {
public:
    std::vector<Estacion> estaciones;
    std::vector<Linea> lineas;
    std::vector<Nodo> nodos;
    std::vector<std::vector<Arista>> adj;

    // Velocidad máxima de la red en km/min (se calcula una vez y se guarda).
    mutable double velocidadMaximaKmMin = 0.0;

    // Busca por id y, si no aparece, por nombre sin tildes ni mayúsculas.
    int indiceEstacion(const std::string& idONombre) const;
    int indiceLinea(const std::string& codigo) const;
    int nodo(int estacion, int linea) const;          // -1 si no existe
    std::vector<int> nodosDeEstacion(int estacion) const;
    double distanciaKm(int estA, int estB) const;
    double velocidadMaxima() const;                   // km por minuto
    int totalAristas() const;
};

// Pasa a minúsculas y quita tildes y diéresis (á -> a, ñ -> n).
std::string normalizarTexto(const std::string& texto);

// Distancia haversine en kilómetros entre dos coordenadas.
double haversineKm(double lat1, double lon1, double lat2, double lon2);

// Convierte un camino de nodos (estación, línea) en la lista de estaciones
// visitadas, sin estaciones consecutivas repetidas.
std::vector<int> estacionesDeRuta(const Graph& g, const std::vector<int>& camino);

// Un tramo de la ruta: dos estaciones, la línea usada y sus minutos. En un
// transbordo, "linea" es la línea a la que se pasa.
struct Tramo {
    int desde = -1;
    int hasta = -1;
    int linea = -1;
    double minutos = 0.0;
    bool transbordo = false;
};

std::vector<Tramo> tramosDeRuta(const Graph& g, const std::vector<int>& camino);
