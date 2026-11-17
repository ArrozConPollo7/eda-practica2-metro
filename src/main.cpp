// main.cpp - menú de consola del navegador de rutas.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "algorithms.hpp"
#include "graph.hpp"
#include "io.hpp"
#include "servidor.hpp"
#include "trie.hpp"

namespace {

Graph grafo;
Trie trie;
Resultado ultimaRuta;
std::string carpetaDatos = "data";
double transbordo = 3.0;  // minutos que cuesta cambiar de línea
bool conBuses = true;     // --metro lo desactiva

std::string minutos(double valor) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1) << valor;
    return os.str();
}

std::string milesimas(double valor) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(3) << valor;
    return os.str();
}

// Los tiempos de búsqueda son de microsegundos: se muestran en us.
std::string microsegundos(double milisegundos) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1) << milisegundos * 1000.0;
    return os.str();
}

const std::string& nombre(int estacion) { return grafo.estaciones[estacion].nombre; }

void prepararTrie() {
    trie = Trie();
    for (std::size_t i = 0; i < grafo.estaciones.size(); ++i) {
        trie.insertar(grafo.estaciones[i].nombre, static_cast<int>(i));
        trie.insertar(grafo.estaciones[i].id, static_cast<int>(i));
    }
}

bool leerEntero(const std::string& texto, int& valor) {
    try {
        std::size_t usado = 0;
        valor = std::stoi(texto, &usado);
        return usado == texto.size();
    } catch (...) {
        return false;
    }
}

bool leerDouble(const std::string& texto, double& valor) {
    try {
        std::size_t usado = 0;
        valor = std::stod(texto, &usado);
        return usado == texto.size();
    } catch (...) {
        return false;
    }
}

// Pide una estación por prefijo y deja elegir entre las coincidencias del Trie.
int pedirEstacion(const std::string& etiqueta) {
    while (true) {
        std::cout << etiqueta << " (prefijo o nombre, vacío cancela): ";
        std::string texto;
        if (!std::getline(std::cin, texto)) return -1;
        if (texto.empty()) return -1;

        int numero = -1;
        if (leerEntero(texto, numero) && numero >= 0 &&
            numero < static_cast<int>(grafo.estaciones.size())) {
            return numero;
        }

        const std::vector<int> coincidencias = trie.porPrefijo(texto, 8);
        if (coincidencias.empty()) {
            std::cout << "  Ninguna estación empieza así.\n";
            continue;
        }
        if (coincidencias.size() == 1) return coincidencias.front();

        std::cout << "  Coincidencias:\n";
        for (std::size_t i = 0; i < coincidencias.size(); ++i) {
            std::cout << "   " << (i + 1) << ") " << nombre(coincidencias[i]) << "\n";
        }
        std::cout << "  Elige (0 cancela): ";
        std::string eleccion;
        if (!std::getline(std::cin, eleccion)) return -1;
        int indice = -1;
        if (!leerEntero(eleccion, indice) || indice < 0 ||
            indice > static_cast<int>(coincidencias.size())) {
            std::cout << "  Opción inválida.\n";
            continue;
        }
        if (indice == 0) return -1;
        return coincidencias[indice - 1];
    }
}

void imprimirRuta(const Resultado& r) {
    if (!r.encontrado) {
        std::cout << "No hay ruta entre esas estaciones.\n";
        return;
    }
    const std::vector<Tramo> tramos = tramosDeRuta(grafo, r.nodos);
    const int origen = grafo.nodos[r.nodos.front()].estacion;
    const int destino = grafo.nodos[r.nodos.back()].estacion;

    std::cout << "\nRuta " << nombre(origen) << " -> " << nombre(destino) << "\n";
    std::cout << "  " << nombre(origen) << "\n";
    double acumulado = 0.0;
    int cambios = 0;
    int viajes = 0;
    for (const Tramo& t : tramos) {
        acumulado += t.minutos;
        if (t.transbordo) {
            ++cambios;
            std::cout << "  -- transbordo a Línea " << grafo.lineas[t.linea].codigo << " (+"
                      << minutos(t.minutos) << " min) en " << nombre(t.desde) << "\n";
        } else {
            ++viajes;
            std::cout << "  " << nombre(t.hasta) << "  [Línea " << grafo.lineas[t.linea].codigo
                      << "]  " << minutos(acumulado) << " min\n";
        }
    }
    std::cout << "Total: " << minutos(r.costo) << " min | " << viajes << " tramos | " << cambios
              << " transbordos | " << estacionesDeRuta(grafo, r.nodos).size() << " estaciones\n";
    std::cout << "Nodos explorados: " << r.explorados << " | tiempo de búsqueda: "
              << milesimas(r.ms) << " ms\n";
}

void opcionBuscar() {
    std::cout << "Prefijo a autocompletar: ";
    std::string texto;
    if (!std::getline(std::cin, texto) || texto.empty()) return;
    const std::vector<int> coincidencias = trie.porPrefijo(texto, 10);
    if (coincidencias.empty()) {
        std::cout << "  Sin coincidencias.\n";
        return;
    }
    std::cout << "  " << coincidencias.size() << " coincidencia(s):\n";
    for (int i : coincidencias) {
        std::cout << "   - " << nombre(i) << " [";
        for (std::size_t j = 0; j < grafo.estaciones[i].lineas.size(); ++j) {
            if (j > 0) std::cout << ", ";
            std::cout << grafo.lineas[grafo.estaciones[i].lineas[j]].codigo;
        }
        std::cout << "]\n";
    }
}

void opcionRuta() {
    const int origen = pedirEstacion("Origen");
    if (origen < 0) return;
    const int destino = pedirEstacion("Destino");
    if (destino < 0) return;
    ultimaRuta = dijkstra(grafo, grafo.nodosDeEstacion(origen), grafo.nodosDeEstacion(destino));
    imprimirRuta(ultimaRuta);
}

void opcionComparar() {
    const int origen = pedirEstacion("Origen");
    if (origen < 0) return;
    const int destino = pedirEstacion("Destino");
    if (destino < 0) return;

    const std::vector<int> origenes = grafo.nodosDeEstacion(origen);
    const std::vector<int> destinos = grafo.nodosDeEstacion(destino);
    const Resultado d = dijkstra(grafo, origenes, destinos);
    const Resultado a = astar(grafo, origenes, destinos);
    ultimaRuta = d;

    std::cout << "\nPar consultado: " << nombre(origen) << " -> " << nombre(destino) << "\n";
    std::cout << "  Dijkstra: costo " << minutos(d.costo) << " min, " << d.explorados
              << " nodos explorados, " << milesimas(d.ms) << " ms\n";
    std::cout << "  A*:       costo " << minutos(a.costo) << " min, " << a.explorados
              << " nodos explorados, " << milesimas(a.ms) << " ms\n";
    // Promedio sobre 200 corridas: una sola medición en un grafo de 53 nodos
    // está en el límite de resolución del cronómetro.
    const int repeticiones = 200;
    double tiempoDijkstra = 0.0;
    double tiempoAstar = 0.0;
    for (int i = 0; i < repeticiones; ++i) {
        tiempoDijkstra += dijkstra(grafo, origenes, destinos).ms;
        tiempoAstar += astar(grafo, origenes, destinos).ms;
    }
    std::cout << "  Promedio de " << repeticiones << " corridas: Dijkstra "
              << microsegundos(tiempoDijkstra / repeticiones) << " us, A* "
              << microsegundos(tiempoAstar / repeticiones) << " us\n";

    // Veinte pares aleatorios con semilla fija: el experimento se repite igual.
    const int total = 20;
    std::mt19937 generador(12345);
    std::uniform_int_distribution<int> dist(0, static_cast<int>(grafo.estaciones.size()) - 1);

    std::cout << "\n" << total << " pares aleatorios (semilla 12345)\n";
    std::cout << "  #  par                                        costoD  costoA  explD explA"
                 "    usD    usA\n";
    double diferenciaMaxima = 0.0;
    double sumaCostoD = 0.0, sumaCostoA = 0.0, sumaMsD = 0.0, sumaMsA = 0.0;
    long sumaExplD = 0, sumaExplA = 0;
    int corridas = 0;

    for (int i = 0; i < total; ++i) {
        int o = dist(generador);
        int f = dist(generador);
        if (o == f) f = (f + 1) % static_cast<int>(grafo.estaciones.size());

        const Resultado rd = dijkstra(grafo, grafo.nodosDeEstacion(o),
                                      grafo.nodosDeEstacion(f));
        const Resultado ra = astar(grafo, grafo.nodosDeEstacion(o),
                                   grafo.nodosDeEstacion(f));
        if (!rd.encontrado || !ra.encontrado) continue;

        ++corridas;
        sumaCostoD += rd.costo;
        sumaCostoA += ra.costo;
        sumaMsD += rd.ms;
        sumaMsA += ra.ms;
        sumaExplD += rd.explorados;
        sumaExplA += ra.explorados;
        diferenciaMaxima = std::max(diferenciaMaxima, std::fabs(rd.costo - ra.costo));

        std::ostringstream par;
        par << nombre(o) << " -> " << nombre(f);
        std::cout << std::setw(3) << (i + 1) << "  " << std::left << std::setw(43) << par.str()
                  << std::right << std::fixed << std::setprecision(1) << std::setw(7) << rd.costo
                  << std::setw(8) << ra.costo << std::setw(7) << rd.explorados << std::setw(6)
                  << ra.explorados << std::setw(8) << rd.ms * 1000.0 << std::setw(8)
                  << ra.ms * 1000.0 << "\n";
    }

    if (corridas == 0) return;
    std::cout << "\nResumen de " << corridas << " pares\n";
    std::cout << "  diferencia máxima de costo: " << milesimas(diferenciaMaxima) << " min\n";
    std::cout << "  explorados promedio: Dijkstra " << (sumaExplD / corridas) << ", A* "
              << (sumaExplA / corridas) << " (" << std::fixed << std::setprecision(1)
              << (100.0 * static_cast<double>(sumaExplA) / static_cast<double>(sumaExplD))
              << " % del total)\n";
    std::cout << "  tiempo promedio: Dijkstra " << microsegundos(sumaMsD / corridas) << " us, A* "
              << microsegundos(sumaMsA / corridas) << " us\n";
    std::cout << "  costo promedio: Dijkstra " << minutos(sumaCostoD / corridas) << " min, A* "
              << minutos(sumaCostoA / corridas) << " min\n";
}

void opcionAlternativas() {
    const int origen = pedirEstacion("Origen");
    if (origen < 0) return;
    const int destino = pedirEstacion("Destino");
    if (destino < 0) return;

    std::cout << "Cuántas rutas (2-6, Enter para 3): ";
    std::string texto;
    if (!std::getline(std::cin, texto)) return;
    int k = 3;
    if (!texto.empty() && (!leerEntero(texto, k) || k < 2 || k > 6)) {
        std::cout << "Valor inválido; se usan 3 rutas.\n";
        k = 3;
    }

    const Resultado base = dijkstra(grafo, grafo.nodosDeEstacion(origen),
                                    grafo.nodosDeEstacion(destino));
    if (!base.encontrado) {
        std::cout << "No hay ruta entre esas estaciones.\n";
        return;
    }
    const std::vector<Resultado> rutas =
        yen(grafo, base.nodos.front(), base.nodos.back(), k);
    std::cout << "\n" << rutas.size() << " rutas sin ciclos de " << nombre(origen) << " a "
              << nombre(destino) << "\n";
    for (std::size_t i = 0; i < rutas.size(); ++i) {
        const std::vector<int> estaciones = estacionesDeRuta(grafo, rutas[i].nodos);
        std::cout << "  " << (i + 1) << ") " << minutos(rutas[i].costo) << " min, "
                  << estaciones.size() << " estaciones: ";
        for (std::size_t j = 0; j < estaciones.size(); ++j) {
            if (j > 0) std::cout << " - ";
            std::cout << nombre(estaciones[j]);
        }
        std::cout << "\n";
    }
}

void opcionCerrada() {
    const int cerrada = pedirEstacion("Estación que se cierra");
    if (cerrada < 0) return;
    const int origen = pedirEstacion("Origen");
    if (origen < 0) return;
    const int destino = pedirEstacion("Destino");
    if (destino < 0) return;

    Bloqueo bloqueo(static_cast<int>(grafo.nodos.size()));
    for (int n : grafo.nodosDeEstacion(cerrada)) bloqueo.nodos[n] = 1;

    const Resultado antes = dijkstra(grafo, grafo.nodosDeEstacion(origen),
                                     grafo.nodosDeEstacion(destino));
    const Resultado despues = dijkstra(grafo, grafo.nodosDeEstacion(origen),
                                       grafo.nodosDeEstacion(destino), &bloqueo);

    std::cout << "\nEstación cerrada: " << nombre(cerrada) << "\n";
    if (!antes.encontrado) {
        std::cout << "  Ya sin la simulación no había ruta.\n";
        return;
    }
    std::cout << "  Sin cierre: " << minutos(antes.costo) << " min\n";
    if (!despues.encontrado) {
        std::cout << "  Con el cierre: sin ruta posible entre esas estaciones\n";
        return;
    }
    std::cout << "  Con el cierre: " << minutos(despues.costo) << " min ("
              << minutos(despues.costo - antes.costo) << " min más)\n";
    imprimirRuta(despues);
    ultimaRuta = despues;
}

void opcionArticulacion() {
    const std::vector<int> puntos = puntosDeArticulacion(grafo);
    std::cout << "\nPuntos de articulación (su cierre parte la red en dos): " << puntos.size()
              << "\n";
    for (int p : puntos) {
        std::cout << "  - " << nombre(p) << " [";
        for (std::size_t j = 0; j < grafo.estaciones[p].lineas.size(); ++j) {
            if (j > 0) std::cout << ", ";
            std::cout << grafo.lineas[grafo.estaciones[p].lineas[j]].codigo;
        }
        std::cout << "]\n";
    }
}

void opcionExportar() {
    std::filesystem::create_directories("out");
    const Resultado* seleccion = ultimaRuta.encontrado ? &ultimaRuta : nullptr;
    const std::string base = seleccion != nullptr ? "out/route" : "out/grafo";

    const bool dot = exportarDot(base + ".dot", grafo, seleccion);
    const bool json = exportarJson(base + ".json", grafo, seleccion);
    const bool svg = exportarSvg(base + ".svg", grafo, seleccion);
    std::cout << "\n" << (dot ? "OK " : "FALLÓ ") << base << ".dot\n"
              << (json ? "OK " : "FALLÓ ") << base << ".json\n"
              << (svg ? "OK " : "FALLÓ ") << base << ".svg\n";
    if (seleccion == nullptr) {
        std::cout << "(no hay ruta seleccionada: se exportó el grafo completo)\n";
    }
}

void opcionTransbordo() {
    std::cout << "Penalización de transbordo en minutos (0 desactiva el modelo): ";
    std::string texto;
    if (!std::getline(std::cin, texto)) return;
    double valor = 0.0;
    if (!leerDouble(texto, valor) || valor < 0.0) {
        std::cout << "Valor inválido.\n";
        return;
    }
    Graph nuevo;
    if (!cargarGrafo(carpetaDatos, valor, nuevo, conBuses)) {
        std::cout << "No se pudo recargar el grafo; se mantiene la penalización anterior.\n";
        return;
    }
    grafo = std::move(nuevo);
    transbordo = valor;
    prepararTrie();
    ultimaRuta = Resultado{};
    std::cout << "Listo: transbordo de " << minutos(transbordo) << " min.\n";
}

void mostrarMenu() {
    std::cout << "\n=== Navegador de rutas: Metro de Medellín ===\n"
              << " 1) Autocompletar estación (Trie)\n"
              << " 2) Ruta más rápida (Dijkstra)\n"
              << " 3) Comparar A* contra Dijkstra\n"
              << " 4) Rutas alternativas (Yen)\n"
              << " 5) Simular estación cerrada\n"
              << " 6) Puntos de articulación de la red\n"
              << " 7) Exportar grafo y ruta (DOT, JSON, SVG)\n"
              << " 8) Cambiar penalización de transbordo (" << minutos(transbordo) << " min)\n"
              << " 0) Salir\n"
              << "Opción: ";
}

}  // namespace

int main(int argc, char** argv) {
    // Modo web: navegador --web [puerto] [carpeta_datos]
    bool modoWeb = false;
    int puerto = 8080;
    std::string carpeta = "data";
    std::vector<std::string> argumentos;
    for (int i = 1; i < argc; ++i) argumentos.push_back(argv[i]);
    // --metro deja el grafo solo con las líneas del Metro (sin buses): sirve
    // para comparar los dos modelos sin cambiar los datos.
    for (std::size_t i = 0; i < argumentos.size();) {
        if (argumentos[i] == "--metro") {
            conBuses = false;
            argumentos.erase(argumentos.begin() + static_cast<long>(i));
        } else {
            ++i;
        }
    }
    if (!argumentos.empty() && argumentos.front() == "--web") {
        modoWeb = true;
        argumentos.erase(argumentos.begin());
        if (!argumentos.empty() && !argumentos.front().empty() &&
            argumentos.front().find_first_not_of("0123456789") == std::string::npos) {
            puerto = std::stoi(argumentos.front());
            argumentos.erase(argumentos.begin());
        }
    }
    if (!argumentos.empty()) carpeta = argumentos.front();

    carpetaDatos = carpeta;
    if (!cargarGrafo(carpeta, transbordo, grafo, conBuses)) {
        carpetaDatos = "../" + carpeta;
        if (!cargarGrafo(carpetaDatos, transbordo, grafo, conBuses)) {
            std::cerr << "No se encontraron stations.csv y edges.csv en '" << carpeta
                      << "' (uso: navegador [--web [puerto]] [--metro] [carpeta_datos])\n";
            return 1;
        }
    }
    if (modoWeb) return iniciarServidor(carpetaDatos, transbordo, puerto, conBuses);
    prepararTrie();

    int paradas = 0;
    std::vector<char> lineaDeBus(grafo.lineas.size(), 0);
    for (const Estacion& e : grafo.estaciones) {
        if (!e.bus) continue;
        ++paradas;
        for (int l : e.lineas) lineaDeBus[static_cast<std::size_t>(l)] = 1;
    }
    int rutasBus = 0;
    for (char c : lineaDeBus) {
        if (c != 0) ++rutasBus;
    }
    std::cout << "Metro de Medellín: " << (grafo.estaciones.size() - paradas) << " estaciones, "
              << (grafo.lineas.size() - rutasBus) << " líneas del Metro, " << paradas
              << " paradas de bus en " << rutasBus << " rutas, " << grafo.nodos.size()
              << " nodos (estación, línea), " << grafo.totalAristas() << " aristas dirigidas\n";

    while (true) {
        mostrarMenu();
        std::string opcion;
        if (!std::getline(std::cin, opcion)) break;
        if (opcion == "0") break;
        if (opcion == "1") opcionBuscar();
        else if (opcion == "2") opcionRuta();
        else if (opcion == "3") opcionComparar();
        else if (opcion == "4") opcionAlternativas();
        else if (opcion == "5") opcionCerrada();
        else if (opcion == "6") opcionArticulacion();
        else if (opcion == "7") opcionExportar();
        else if (opcion == "8") opcionTransbordo();
        else std::cout << "Opción no válida.\n";
    }
    std::cout << "Hasta luego.\n";
    return 0;
}
