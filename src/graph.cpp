// graph.cpp - consultas sobre el grafo ya construido.
#include "graph.hpp"

#include <cmath>

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double RADIO_TIERRA_KM = 6371.0;
}  // namespace

std::string normalizarTexto(const std::string& texto) {
    // Se reemplazan las tildes por su vocal y luego se pasa todo a minúscula.
    std::string salida;
    salida.reserve(texto.size());
    for (unsigned char c : texto) {
        switch (c) {
            case 'a': case 'A': salida.push_back('a'); break;
            case 'e': case 'E': salida.push_back('e'); break;
            case 'i': case 'I': salida.push_back('i'); break;
            case 'o': case 'O': salida.push_back('o'); break;
            case 'u': case 'U': salida.push_back('u'); break;
            case 'n': case 'N': salida.push_back('n'); break;
            case 0xC3: break;  // primer byte de á é í ó ú ñ ü: se ignora
            case 0xA1: case 0x81: salida.push_back('a'); break;  // á Á
            case 0xA9: case 0x89: salida.push_back('e'); break;  // é É
            case 0xAD: case 0x8D: salida.push_back('i'); break;  // í Í
            case 0xB3: case 0x93: salida.push_back('o'); break;  // ó Ó
            case 0xBA: case 0xBC: case 0x9A: case 0x9C:
                salida.push_back('u'); break;                    // ú ü Ú Ü
            case 0xB1: case 0x91: salida.push_back('n'); break;  // ñ Ñ
            default:
                if (c >= 'A' && c <= 'Z') {
                    salida.push_back(static_cast<char>(c - 'A' + 'a'));
                } else {
                    salida.push_back(static_cast<char>(c));
                }
        }
    }
    return salida;
}

double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    const double p1 = lat1 * PI / 180.0;
    const double p2 = lat2 * PI / 180.0;
    const double dp = (lat2 - lat1) * PI / 180.0;
    const double dl = (lon2 - lon1) * PI / 180.0;
    const double h = std::sin(dp / 2) * std::sin(dp / 2) +
                     std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
    return 2.0 * RADIO_TIERRA_KM * std::asin(std::sqrt(h));
}

int Graph::indiceEstacion(const std::string& idONombre) const {
    for (std::size_t i = 0; i < estaciones.size(); ++i) {
        if (estaciones[i].id == idONombre) return static_cast<int>(i);
    }
    const std::string buscado = normalizarTexto(idONombre);
    for (std::size_t i = 0; i < estaciones.size(); ++i) {
        if (normalizarTexto(estaciones[i].nombre) == buscado) return static_cast<int>(i);
    }
    return -1;
}

int Graph::indiceLinea(const std::string& codigo) const {
    const std::string buscado = normalizarTexto(codigo);
    for (std::size_t i = 0; i < lineas.size(); ++i) {
        if (normalizarTexto(lineas[i].codigo) == buscado) return static_cast<int>(i);
    }
    return -1;
}

int Graph::nodo(int estacion, int linea) const {
    for (std::size_t i = 0; i < nodos.size(); ++i) {
        if (nodos[i].estacion == estacion && nodos[i].linea == linea) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::vector<int> Graph::nodosDeEstacion(int estacion) const {
    std::vector<int> salida;
    for (std::size_t i = 0; i < nodos.size(); ++i) {
        if (nodos[i].estacion == estacion) salida.push_back(static_cast<int>(i));
    }
    return salida;
}

double Graph::distanciaKm(int estA, int estB) const {
    return haversineKm(estaciones[estA].lat, estaciones[estA].lon,
                       estaciones[estB].lat, estaciones[estB].lon);
}

double Graph::velocidadMaxima() const {
    // Se mide en las aristas del archivo y se suma un 5 % de margen para
    // absorber el redondeo de los minutos. Con ese margen la heurística nunca
    // sobreestima el costo real y A* sigue siendo óptimo.
    if (velocidadMaximaKmMin > 0.0) return velocidadMaximaKmMin;
    double maxima = 1.0;
    for (std::size_t u = 0; u < adj.size(); ++u) {
        for (const Arista& a : adj[u]) {
            if (a.transbordo || a.minutos <= 0.0) continue;
            const double km = distanciaKm(nodos[u].estacion, nodos[a.destino].estacion);
            const double velocidad = km / (a.minutos / 60.0);  // km/h
            if (velocidad > maxima) maxima = velocidad;
        }
    }
    velocidadMaximaKmMin = maxima * 1.05 / 60.0;  // km por minuto
    return velocidadMaximaKmMin;
}

int Graph::totalAristas() const {
    int total = 0;
    for (const std::vector<Arista>& lista : adj) total += static_cast<int>(lista.size());
    return total;
}

std::vector<int> estacionesDeRuta(const Graph& g, const std::vector<int>& camino) {
    std::vector<int> salida;
    for (int n : camino) {
        const int est = g.nodos[n].estacion;
        if (salida.empty() || salida.back() != est) salida.push_back(est);
    }
    return salida;
}

std::vector<Tramo> tramosDeRuta(const Graph& g, const std::vector<int>& camino) {
    std::vector<Tramo> salida;
    for (std::size_t i = 0; i + 1 < camino.size(); ++i) {
        const int u = camino[i];
        const int v = camino[i + 1];
        Tramo t;
        t.desde = g.nodos[u].estacion;
        t.hasta = g.nodos[v].estacion;
        // Busca la arista que une los dos nodos para tomar su costo real.
        double minutos = -1.0;
        bool esTransbordo = false;
        for (const Arista& a : g.adj[u]) {
            if (a.destino != v) continue;
            if (minutos < 0.0 || a.minutos < minutos) {
                minutos = a.minutos;
                esTransbordo = a.transbordo;
            }
        }
        if (minutos < 0.0) continue;
        t.minutos = minutos;
        t.transbordo = esTransbordo;
        t.linea = esTransbordo ? g.nodos[v].linea : g.nodos[u].linea;
        salida.push_back(t);
    }
    return salida;
}
