// io.cpp - carga del CSV y exportación a DOT, JSON y SVG (escritos a mano).
#include "io.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace {

constexpr double PI = 3.14159265358979323846;

// Color aproximado de cada línea, tomado de los colores distintivos del sistema.
std::string colorDeLinea(const std::string& codigo) {
    if (codigo == "A") return "#0065B3";
    if (codigo == "B") return "#F58220";
    if (codigo == "K") return "#C1D72F";
    if (codigo == "J") return "#FFCD00";
    if (codigo == "L") return "#9A681C";
    if (codigo == "H") return "#FF0061";
    if (codigo == "M") return "#622081";
    if (codigo == "T-A") return "#15C240";
    // Cualquier otro código es una ruta alimentadora: gris claro para que no
    // compita visualmente con las líneas del Metro.
    return "#9AA0A6";
}

std::string recortar(std::string texto) {
    while (!texto.empty() && (texto.back() == '\r' || texto.back() == ' ')) texto.pop_back();
    return texto;
}

std::vector<std::string> dividir(const std::string& linea, char separador) {
    std::vector<std::string> campos;
    std::string actual;
    for (char c : linea) {
        if (c == separador) {
            campos.push_back(recortar(actual));
            actual.clear();
        } else {
            actual.push_back(c);
        }
    }
    campos.push_back(recortar(actual));
    return campos;
}

std::string numero(double valor) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1) << valor;
    return os.str();
}

std::string escaparXml(const std::string& texto) {
    std::string salida;
    for (char c : texto) {
        if (c == '&') salida += "&amp;";
        else if (c == '<') salida += "&lt;";
        else if (c == '>') salida += "&gt;";
        else salida.push_back(c);
    }
    return salida;
}

// Identificador del nodo para DOT y JSON: "estacion|linea".
std::string claveNodo(const Graph& g, int n) {
    return g.estaciones[g.nodos[n].estacion].id + "|" + g.lineas[g.nodos[n].linea].codigo;
}

bool esDelCamino(const std::vector<int>& camino, int u, int v) {
    for (std::size_t i = 0; i + 1 < camino.size(); ++i) {
        if (camino[i] == u && camino[i + 1] == v) return true;
    }
    return false;
}

bool estaEnCamino(const std::vector<int>& camino, int nodo) {
    return std::find(camino.begin(), camino.end(), nodo) != camino.end();
}

}  // namespace

namespace {

// Índices para no recorrer todo el grafo por cada fila de los CSV: con los
// buses urbanos hay ~12.000 estaciones y ~22.000 aristas, y las búsquedas
// lineales de Graph::indiceEstacion/Graph::nodo se vuelven el cuello de botella.
struct Indices {
    std::unordered_map<std::string, int> estacionPorId;
    std::unordered_map<std::string, int> lineaPorCodigo;
    std::unordered_map<long long, int> nodoPorPar;
    std::vector<std::vector<int>> nodosPorEstacion;
};

// Carga data/<prefijo>_paradas.csv como estaciones de la línea de su ruta.
void cargarParadas(const std::string& dirDatos, const std::string& prefijo, Graph& g) {
    std::ifstream paradas(dirDatos + "/" + prefijo + "_paradas.csv");
    if (!paradas) return;
    std::string fila;
    std::getline(paradas, fila);  // encabezado
    while (std::getline(paradas, fila)) {
        if (recortar(fila).empty()) continue;
        const std::vector<std::string> c = dividir(fila, ',');
        if (c.size() < 5) continue;
        int indice = g.indiceLinea(c[4]);
        if (indice < 0) {
            g.lineas.push_back({c[4], colorDeLinea(c[4])});
            indice = static_cast<int>(g.lineas.size()) - 1;
        }
        Estacion p;
        p.id = c[0];
        p.nombre = c[1];
        p.lat = std::stod(c[2]);
        p.lon = std::stod(c[3]);
        p.lineas.push_back(indice);
        p.bus = true;
        g.estaciones.push_back(p);
    }
}

// Carga los tramos entre paradas y los accesos a pie de un conjunto de rutas.
// El acceso se marca como transbordo porque cambia de línea.
void cargarAristasDeBus(const std::string& dirDatos, const std::string& prefijo, Graph& g,
                        const Indices& ix) {
    auto estacion = [&ix](const std::string& id) {
        const auto it = ix.estacionPorId.find(id);
        return it == ix.estacionPorId.end() ? -1 : it->second;
    };
    auto nodo = [&ix](int est, int lin) {
        const auto it = ix.nodoPorPar.find(static_cast<long long>(est) * 1000 + lin);
        return it == ix.nodoPorPar.end() ? -1 : it->second;
    };

    std::ifstream tramos(dirDatos + "/" + prefijo + "_tramos.csv");
    if (tramos) {
        std::string fila;
        std::getline(tramos, fila);  // encabezado
        while (std::getline(tramos, fila)) {
            if (recortar(fila).empty()) continue;
            const std::vector<std::string> c = dividir(fila, ',');
            if (c.size() < 4) continue;
            const int a = estacion(c[0]);
            const int b = estacion(c[1]);
            const auto lin = ix.lineaPorCodigo.find(c[3]);
            if (a < 0 || b < 0 || lin == ix.lineaPorCodigo.end()) continue;
            const int u = nodo(a, lin->second);
            const int v = nodo(b, lin->second);
            if (u < 0 || v < 0) continue;
            g.adj[static_cast<std::size_t>(u)].push_back({v, std::stod(c[2]), false});
        }
    }

    // El CSV ya trae los dos sentidos; el de subida al bus incluye la espera.
    std::ifstream accesos(dirDatos + "/" + prefijo + "_acceso.csv");
    if (accesos) {
        std::string fila;
        std::getline(accesos, fila);  // encabezado
        while (std::getline(accesos, fila)) {
            if (recortar(fila).empty()) continue;
            const std::vector<std::string> c = dividir(fila, ',');
            if (c.size() < 3) continue;
            const int a = estacion(c[0]);
            const int b = estacion(c[1]);
            if (a < 0 || b < 0) continue;
            const double minutos = std::stod(c[2]);
            for (int desde : ix.nodosPorEstacion[static_cast<std::size_t>(a)]) {
                for (int hasta : ix.nodosPorEstacion[static_cast<std::size_t>(b)]) {
                    g.adj[static_cast<std::size_t>(desde)].push_back({hasta, minutos, true});
                }
            }
        }
    }
}

}  // namespace

bool cargarGrafo(const std::string& dirDatos, double transbordo, Graph& g, bool conBuses) {
    g = Graph{};

    std::ifstream estaciones(dirDatos + "/stations.csv");
    if (!estaciones) return false;  // main avisa si no encuentra la carpeta

    std::string linea;
    std::getline(estaciones, linea);  // encabezado
    while (std::getline(estaciones, linea)) {
        if (recortar(linea).empty()) continue;
        const std::vector<std::string> campos = dividir(linea, ',');
        if (campos.size() < 5) continue;
        Estacion e;
        e.id = campos[0];
        e.nombre = campos[1];
        for (const std::string& codigo : dividir(campos[2], '|')) {
            if (codigo.empty()) continue;
            int indice = g.indiceLinea(codigo);
            if (indice < 0) {
                g.lineas.push_back({codigo, colorDeLinea(codigo)});
                indice = static_cast<int>(g.lineas.size()) - 1;
            }
            e.lineas.push_back(indice);
        }
        e.lat = std::stod(campos[3]);
        e.lon = std::stod(campos[4]);
        g.estaciones.push_back(e);
    }
    estaciones.close();

    // Rutas de bus: alimentadoras del Metro y urbanas de la Alcaldía. Cada
    // parada entra como una estación de la línea de su ruta.
    if (conBuses) {
        cargarParadas(dirDatos, "bus", g);
        cargarParadas(dirDatos, "urb", g);
    }

    if (g.estaciones.empty()) return false;

    // Un nodo por cada par (estación, línea).
    for (std::size_t i = 0; i < g.estaciones.size(); ++i) {
        for (int lin : g.estaciones[i].lineas) {
            g.nodos.push_back({static_cast<int>(i), lin});
        }
    }
    g.adj.assign(g.nodos.size(), {});

    // Índices una sola vez: las búsquedas lineales de Graph no aguantan 12.000
    // estaciones con 22.000 aristas.
    Indices ix;
    ix.estacionPorId.reserve(g.estaciones.size() * 2);
    ix.lineaPorCodigo.reserve(g.lineas.size() * 2);
    ix.nodoPorPar.reserve(g.nodos.size() * 2);
    ix.nodosPorEstacion.assign(g.estaciones.size(), {});
    for (std::size_t i = 0; i < g.estaciones.size(); ++i) {
        ix.estacionPorId[g.estaciones[i].id] = static_cast<int>(i);
    }
    for (std::size_t i = 0; i < g.lineas.size(); ++i) {
        ix.lineaPorCodigo[g.lineas[i].codigo] = static_cast<int>(i);
    }
    for (std::size_t n = 0; n < g.nodos.size(); ++n) {
        const int est = g.nodos[n].estacion;
        const int lin = g.nodos[n].linea;
        ix.nodoPorPar[static_cast<long long>(est) * 1000 + lin] = static_cast<int>(n);
        ix.nodosPorEstacion[static_cast<std::size_t>(est)].push_back(static_cast<int>(n));
    }

    std::ifstream aristas(dirDatos + "/edges.csv");
    if (!aristas) return false;
    std::getline(aristas, linea);  // encabezado
    int saltadas = 0;
    while (std::getline(aristas, linea)) {
        if (recortar(linea).empty()) continue;
        const std::vector<std::string> campos = dividir(linea, ',');
        if (campos.size() < 3) continue;
        const auto itA = ix.estacionPorId.find(campos[0]);
        const auto itB = ix.estacionPorId.find(campos[1]);
        const int a = itA == ix.estacionPorId.end() ? -1 : itA->second;
        const int b = itB == ix.estacionPorId.end() ? -1 : itB->second;
        if (a < 0 || b < 0) {
            ++saltadas;
            continue;
        }
        const double minutos = std::stod(campos[2]);
        // La arista pertenece a la línea que comparten las dos estaciones; en
        // este sistema siempre es exactamente una.
        for (int lin : g.estaciones[a].lineas) {
            if (g.estaciones[b].lineas.end() ==
                std::find(g.estaciones[b].lineas.begin(), g.estaciones[b].lineas.end(), lin)) {
                continue;
            }
            const auto itU = ix.nodoPorPar.find(static_cast<long long>(a) * 1000 + lin);
            const auto itV = ix.nodoPorPar.find(static_cast<long long>(b) * 1000 + lin);
            if (itU == ix.nodoPorPar.end() || itV == ix.nodoPorPar.end()) continue;
            const int u = itU->second;
            const int v = itV->second;
            g.adj[u].push_back({v, minutos, false});
            g.adj[v].push_back({u, minutos, false});
        }
    }
    aristas.close();
    if (saltadas > 0) {
        std::cerr << "Aviso: " << saltadas << " aristas con estaciones desconocidas\n";
    }

    // Aristas de transbordo: cambiar de línea dentro de la misma estación.
    for (std::size_t i = 0; i < g.estaciones.size(); ++i) {
        for (int desde : g.estaciones[i].lineas) {
            for (int hasta : g.estaciones[i].lineas) {
                if (desde == hasta) continue;
                const auto itU = ix.nodoPorPar.find(static_cast<long long>(i) * 1000 + desde);
                const auto itV = ix.nodoPorPar.find(static_cast<long long>(i) * 1000 + hasta);
                if (itU == ix.nodoPorPar.end() || itV == ix.nodoPorPar.end()) continue;
                g.adj[itU->second].push_back({itV->second, transbordo, true});
            }
        }
    }
    // Rutas de bus: tramos entre paradas consecutivas y acceso a pie a las
    // estaciones. El acceso se marca como transbordo porque cambia de línea.
    if (conBuses) {
        cargarAristasDeBus(dirDatos, "bus", g, ix);
        cargarAristasDeBus(dirDatos, "urb", g, ix);
    }

    // Se deja calculada la velocidad máxima, que A* usa en cada búsqueda.
    g.velocidadMaxima();
    return true;
}

bool exportarDot(const std::string& ruta, const Graph& g, const Resultado* seleccion) {
    std::ofstream salida(ruta);
    if (!salida) return false;
    const std::vector<int> camino = seleccion != nullptr ? seleccion->nodos : std::vector<int>{};

    salida << "// Grafo del Metro de Medellín generado por el navegador de rutas.\n";
    salida << "digraph metro {\n";
    salida << "  graph [rankdir=LR, fontname=\"Helvetica\", fontsize=12, labelloc=t, label=\"";
    if (seleccion != nullptr && !camino.empty()) {
        salida << "Ruta " << g.estaciones[g.nodos[camino.front()].estacion].nombre << " - "
               << g.estaciones[g.nodos[camino.back()].estacion].nombre << " ("
               << numero(seleccion->costo) << " min)";
    } else {
        salida << "Red del Metro de Medellín";
    }
    salida << "\"];\n";
    salida << "  node [shape=circle, style=filled, fontname=\"Helvetica\", fontsize=9, "
              "fillcolor=white];\n";
    salida << "  edge [fontname=\"Helvetica\", fontsize=8, dir=none, color=\"#999999\"];\n\n";

    for (std::size_t u = 0; u < g.nodos.size(); ++u) {
        const std::string color = g.lineas[g.nodos[u].linea].color;
        salida << "  \"" << claveNodo(g, static_cast<int>(u)) << "\" [label=\""
               << escaparXml(g.estaciones[g.nodos[u].estacion].nombre) << "\", color=\""
               << color << "\", penwidth=1.6";
        if (estaEnCamino(camino, static_cast<int>(u))) {
            salida << ", fillcolor=\"#FFF2B2\", penwidth=2.6";
        }
        salida << "];\n";
    }
    salida << "\n";

    std::set<std::pair<int, int>> vistas;
    for (std::size_t u = 0; u < g.nodos.size(); ++u) {
        for (const Arista& a : g.adj[u]) {
            if (static_cast<int>(u) > a.destino) continue;  // una arista por par
            if (!vistas.insert({static_cast<int>(u), a.destino}).second) continue;
            const bool enRuta = esDelCamino(camino, static_cast<int>(u), a.destino) ||
                                esDelCamino(camino, a.destino, static_cast<int>(u));
            salida << "  \"" << claveNodo(g, static_cast<int>(u)) << "\" -> \""
                   << claveNodo(g, a.destino) << "\" [label=\"" << numero(a.minutos) << "\"";
            if (enRuta) {
                salida << ", color=\"#111111\", penwidth=3.0, fontcolor=\"#111111\", dir=forward";
            } else if (a.transbordo) {
                salida << ", style=dashed, color=\"#BBBBBB\"";  // transbordo
            } else {
                salida << ", color=\"" << g.lineas[g.nodos[u].linea].color << "\"";
            }
            salida << "];\n";
        }
    }
    salida << "}\n";
    return true;
}

bool exportarJson(const std::string& ruta, const Graph& g, const Resultado* seleccion) {
    std::ofstream salida(ruta);
    if (!salida) return false;

    salida << "{\n  \"estaciones\": [\n";
    for (std::size_t i = 0; i < g.estaciones.size(); ++i) {
        const Estacion& e = g.estaciones[i];
        salida << "    {\"id\": \"" << e.id << "\", \"nombre\": \"" << e.nombre
               << "\", \"lineas\": [";
        for (std::size_t j = 0; j < e.lineas.size(); ++j) {
            if (j > 0) salida << ", ";
            salida << "\"" << g.lineas[e.lineas[j]].codigo << "\"";
        }
        salida << "], \"lat\": " << std::fixed << std::setprecision(5) << e.lat
               << ", \"lon\": " << e.lon << "}";
        salida << (i + 1 < g.estaciones.size() ? ",\n" : "\n");
    }
    salida << "  ],\n  \"lineas\": [\n";
    for (std::size_t i = 0; i < g.lineas.size(); ++i) {
        salida << "    {\"codigo\": \"" << g.lineas[i].codigo << "\", \"color\": \""
               << g.lineas[i].color << "\"}" << (i + 1 < g.lineas.size() ? ",\n" : "\n");
    }
    salida << "  ],\n  \"aristas\": [\n";

    // Aristas a nivel de estación: los tramos de cada línea y los transbordos.
    std::set<std::tuple<int, int, int>> vistas;
    std::vector<std::string> filas;
    for (std::size_t u = 0; u < g.nodos.size(); ++u) {
        for (const Arista& a : g.adj[u]) {
            const int estA = g.nodos[u].estacion;
            const int estB = g.nodos[a.destino].estacion;
            const int codigoLinea = a.transbordo ? -1 : g.nodos[u].linea;
            const int menor = std::min(estA, estB);
            const int mayor = std::max(estA, estB);
            if (!vistas.insert(std::make_tuple(menor, mayor, codigoLinea)).second) continue;
            std::ostringstream fila;
            fila << "    {\"origen\": \"" << g.estaciones[menor].id << "\", \"destino\": \""
                 << g.estaciones[mayor].id << "\", \"minutos\": " << numero(a.minutos)
                 << ", \"tipo\": \"" << (a.transbordo ? "transbordo" : "via") << "\"";
            if (!a.transbordo) {
                fila << ", \"linea\": \"" << g.lineas[codigoLinea].codigo << "\"";
            }
            fila << "}";
            filas.push_back(fila.str());
        }
    }
    for (std::size_t i = 0; i < filas.size(); ++i) {
        salida << filas[i] << (i + 1 < filas.size() ? ",\n" : "\n");
    }
    salida << "  ],\n";

    if (seleccion == nullptr || seleccion->nodos.empty()) {
        salida << "  \"ruta\": null\n}\n";
        return true;
    }

    salida << "  \"ruta\": {\n";
    salida << "    \"origen\": \"" << g.estaciones[g.nodos[seleccion->nodos.front()].estacion].id
           << "\",\n";
    salida << "    \"destino\": \"" << g.estaciones[g.nodos[seleccion->nodos.back()].estacion].id
           << "\",\n";
    salida << "    \"minutos\": " << numero(seleccion->costo) << ",\n";
    salida << "    \"estaciones\": [";
    const std::vector<int> estacionesRuta = estacionesDeRuta(g, seleccion->nodos);
    for (std::size_t i = 0; i < estacionesRuta.size(); ++i) {
        if (i > 0) salida << ", ";
        salida << "\"" << g.estaciones[estacionesRuta[i]].id << "\"";
    }
    salida << "],\n    \"tramos\": [\n";
    const std::vector<Tramo> tramos = tramosDeRuta(g, seleccion->nodos);
    for (std::size_t i = 0; i < tramos.size(); ++i) {
        const Tramo& t = tramos[i];
        salida << "      {\"desde\": \"" << g.estaciones[t.desde].id << "\", \"hasta\": \""
               << g.estaciones[t.hasta].id << "\", \"minutos\": " << numero(t.minutos);
        if (!t.transbordo) {
            salida << ", \"linea\": \"" << g.lineas[t.linea].codigo << "\"";
        } else {
            salida << ", \"tipo\": \"transbordo\"";
        }
        salida << "}" << (i + 1 < tramos.size() ? ",\n" : "\n");
    }
    salida << "    ]\n  }\n}\n";
    return true;
}

bool exportarSvg(const std::string& ruta, const Graph& g, const Resultado* seleccion) {
    std::ofstream salida(ruta);
    if (!salida) return false;

    double latMin = g.estaciones[0].lat, latMax = latMin;
    double lonMin = g.estaciones[0].lon, lonMax = lonMin;
    for (const Estacion& e : g.estaciones) {
        latMin = std::min(latMin, e.lat);
        latMax = std::max(latMax, e.lat);
        lonMin = std::min(lonMin, e.lon);
        lonMax = std::max(lonMax, e.lon);
    }

    // Proyección plana: se corrige la longitud por el coseno de la latitud para
    // que el mapa no salga estirado, y se invierte la latitud porque en SVG el
    // eje Y crece hacia abajo.
    const double margen = 90.0;
    const double ancho = 1100.0;
    const double cosLat = std::cos((latMin + latMax) / 2.0 * PI / 180.0);
    const double anchoUnidades = (lonMax - lonMin) * cosLat;
    const double altoUnidades = latMax - latMin;
    const double escala = (ancho - 2.0 * margen) / anchoUnidades;
    const double alto = altoUnidades * escala + 2.0 * margen;

    const auto px = [&](double lon) { return margen + (lon - lonMin) * cosLat * escala; };
    const auto py = [&](double lat) { return margen + (latMax - lat) * escala; };

    const std::vector<int> camino = seleccion != nullptr ? seleccion->nodos : std::vector<int>{};
    const std::vector<int> estacionesRuta = estacionesDeRuta(g, camino);

    salida << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    salida << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << static_cast<int>(ancho)
           << "\" height=\"" << static_cast<int>(alto) << "\" viewBox=\"0 0 "
           << static_cast<int>(ancho) << " " << static_cast<int>(alto) << "\">\n";
    salida << "  <rect x=\"0\" y=\"0\" width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";
    salida << "  <text x=\"" << static_cast<int>(margen) << "\" y=\"34\" font-family=\"Helvetica, "
              "Arial, sans-serif\" font-size=\"30\" font-weight=\"bold\" fill=\"#111111\">";
    if (seleccion != nullptr && !camino.empty()) {
        salida << "Metro de Medellín: ruta "
               << escaparXml(g.estaciones[g.nodos[camino.front()].estacion].nombre) << " → "
               << escaparXml(g.estaciones[g.nodos[camino.back()].estacion].nombre) << " ("
               << numero(seleccion->costo) << " min)";
    } else {
        salida << "Metro de Medellín: red completa";
    }
    salida << "</text>\n";

    // Tramos de cada línea, con el color de la línea.
    std::set<std::pair<int, int>> dibujadas;
    for (std::size_t u = 0; u < g.nodos.size(); ++u) {
        for (const Arista& a : g.adj[u]) {
            if (a.transbordo) continue;
            const int estA = g.nodos[u].estacion;
            const int estB = g.nodos[a.destino].estacion;
            if (!dibujadas.insert({std::min(estA, estB), std::max(estA, estB)}).second) continue;
            salida << "  <line x1=\"" << numero(px(g.estaciones[estA].lon)) << "\" y1=\""
                   << numero(py(g.estaciones[estA].lat)) << "\" x2=\""
                   << numero(px(g.estaciones[estB].lon)) << "\" y2=\""
                   << numero(py(g.estaciones[estB].lat)) << "\" stroke=\""
                   << g.lineas[g.nodos[u].linea].color
                   << "\" stroke-width=\"4.8\" stroke-linecap=\"round\" opacity=\"0.55\"/>\n";
        }
    }

    // Estaciones sobre la ruta: polilínea con halo blanco para que destaque.
    if (estacionesRuta.size() > 1) {
        std::ostringstream puntos;
        for (std::size_t i = 0; i < estacionesRuta.size(); ++i) {
            const Estacion& e = g.estaciones[estacionesRuta[i]];
            if (i > 0) puntos << " ";
            puntos << numero(px(e.lon)) << "," << numero(py(e.lat));
        }
        salida << "  <polyline points=\"" << puntos.str()
               << "\" fill=\"none\" stroke=\"#ffffff\" stroke-width=\"15\" "
                  "stroke-linejoin=\"round\" stroke-linecap=\"round\"/>\n";
        salida << "  <polyline points=\"" << puntos.str()
               << "\" fill=\"none\" stroke=\"#14213D\" stroke-width=\"7\" "
                  "stroke-linejoin=\"round\" stroke-linecap=\"round\"/>\n";
    }

    // Puntos de estación: todos en gris y los de la ruta con su color.
    for (std::size_t i = 0; i < g.estaciones.size(); ++i) {
        const Estacion& e = g.estaciones[i];
        const bool enRuta = std::find(estacionesRuta.begin(), estacionesRuta.end(),
                                     static_cast<int>(i)) != estacionesRuta.end();
        const bool intercambio = e.lineas.size() > 1;
        salida << "  <circle cx=\"" << numero(px(e.lon)) << "\" cy=\"" << numero(py(e.lat))
               << "\" r=\"" << (enRuta ? (intercambio ? 9.5 : 7.5) : (intercambio ? 6.5 : 4.5))
               << "\" fill=\"" << (enRuta ? "#14213D" : "#ffffff") << "\" stroke=\""
               << (enRuta ? "#14213D" : g.lineas[e.lineas.front()].color)
               << "\" stroke-width=\"" << (enRuta ? 3.0 : 2.1) << "\"/>\n";
    }

    // Nombres: los de la ruta y los intercambiadores. Si una etiqueta se
    // encimaría con otra ya dibujada, se desplaza unos píxeles hacia arriba o
    // hacia abajo (desplazamiento vertical codicioso).
    std::vector<std::array<double, 4>> ocupados;  // cajas x0, y0, x1, y1
    for (std::size_t i = 0; i < g.estaciones.size(); ++i) {
        const Estacion& e = g.estaciones[i];
        const bool enRuta = std::find(estacionesRuta.begin(), estacionesRuta.end(),
                                     static_cast<int>(i)) != estacionesRuta.end();
        if (!enRuta && e.lineas.size() < 2) continue;
        const double x = px(e.lon);
        const double y = py(e.lat);
        const bool aLaIzquierda = x > ancho - 190.0;
        const double anchoTexto = static_cast<double>(e.nombre.size()) * 6.4 + 4.0;
        const double x0 = aLaIzquierda ? x - 9.0 - anchoTexto : x + 9.0;
        const double x1 = aLaIzquierda ? x - 9.0 : x + 9.0 + anchoTexto;

        double desplazamiento = 0.0;
        for (double intento : {0.0, -14.0, 14.0, -28.0, 28.0, -42.0, 42.0, -56.0, 56.0}) {
            const double y0 = y + 4.0 + intento - 10.0;
            const double y1 = y + 4.0 + intento + 4.0;
            bool choca = false;
            for (const auto& caja : ocupados) {
                if (x0 < caja[2] && caja[0] < x1 && y0 < caja[3] && caja[1] < y1) {
                    choca = true;
                    break;
                }
            }
            if (!choca) {
                desplazamiento = intento;
                ocupados.push_back({x0, y0, x1, y1});
                break;
            }
        }

        const double etiquetaX = aLaIzquierda ? x - 9.0 : x + 9.0;
        const double etiquetaY = y + 4.0 + desplazamiento;
        const int tamano = enRuta ? 16 : 13;
        const std::string color = enRuta ? "#111111" : "#555555";
        const std::string anclaje = aLaIzquierda ? "end" : "start";
        // El halo blanco (texto grueso debajo) mantiene el nombre legible
        // aunque la etiqueta cruce una línea del mapa.
        salida << "  <text x=\"" << numero(etiquetaX) << "\" y=\"" << numero(etiquetaY)
               << "\" font-family=\"Helvetica, Arial, sans-serif\" font-size=\"" << tamano
               << "\" fill=\"#ffffff\" stroke=\"#ffffff\" stroke-width=\"5.0\" "
                  "stroke-linejoin=\"round\" text-anchor=\""
               << anclaje << "\">" << escaparXml(e.nombre) << "</text>\n";
        salida << "  <text x=\"" << numero(etiquetaX) << "\" y=\"" << numero(etiquetaY)
               << "\" font-family=\"Helvetica, Arial, sans-serif\" font-size=\"" << tamano
               << "\" fill=\"" << color << "\" text-anchor=\"" << anclaje << "\"";
        if (!enRuta) salida << " opacity=\"0.85\"";
        salida << ">" << escaparXml(e.nombre) << "</text>\n";
    }

    // Convenciones de línea.
    const double yLeyenda = alto - 34.0;
    double xLeyenda = margen;
    salida << "  <text x=\"" << static_cast<int>(margen) << "\" y=\"" << numero(yLeyenda - 24.0)
           << "\" font-family=\"Helvetica, Arial, sans-serif\" font-size=\"16\" "
              "font-weight=\"bold\" fill=\"#111111\" font-size-adjust=\"none\">Líneas</text>\n";
    for (const Linea& l : g.lineas) {
        salida << "  <rect x=\"" << numero(xLeyenda) << "\" y=\"" << numero(yLeyenda - 10.0)
               << "\" width=\"33\" height=\"10\" fill=\"" << l.color << "\"/>\n";
        salida << "  <text x=\"" << numero(xLeyenda + 39.0) << "\" y=\"" << numero(yLeyenda)
               << "\" font-family=\"Helvetica, Arial, sans-serif\" font-size=\"16\" "
                  "fill=\"#333333\">"
               << escaparXml(l.codigo) << "</text>\n";
        xLeyenda += 93.0;
    }
    if (seleccion != nullptr && !camino.empty()) {
        salida << "  <text x=\"" << numero(xLeyenda + 6.0) << "\" y=\"" << numero(yLeyenda)
               << "\" font-family=\"Helvetica, Arial, sans-serif\" font-size=\"16\" "
                  "fill=\"#14213D\">ruta resaltada</text>\n";
    }
    salida << "</svg>\n";
    return true;
}
