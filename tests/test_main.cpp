// test_main.cpp - pruebas con asserts. No se usa ningún framework.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "algorithms.hpp"
#include "graph.hpp"
#include "io.hpp"
#include "trie.hpp"

namespace {

int estacion(const Graph& g, const std::string& id) {
    const int i = g.indiceEstacion(id);
    assert(i >= 0);
    return i;
}

double costoRuta(const Graph& g, const std::string& desde, const std::string& hasta) {
    const Resultado r = dijkstra(g, g.nodosDeEstacion(estacion(g, desde)),
                                 g.nodosDeEstacion(estacion(g, hasta)));
    assert(r.encontrado);
    return r.costo;
}

bool cerca(double a, double b) { return std::fabs(a - b) < 1e-6; }

}  // namespace

int main() {
    Graph g;
    assert(cargarGrafo(CARPETA_DATOS, 3.0, g, false));

    // 1. Estructura cargada: 46 estaciones, 8 líneas y 53 nodos (estación, línea).
    assert(g.estaciones.size() == 46);
    assert(g.lineas.size() == 8);
    assert(g.nodos.size() == 53);
    std::cout << "OK estructura del grafo\n";

    // 2. Rutas verificadas a mano sumando las aristas de data/edges.csv.
    assert(cerca(costoRuta(g, "niquia", "la_estrella"), 40.2));      // 20 tramos de línea A
    assert(cerca(costoRuta(g, "niquia", "san_antonio"), 18.6));      // 10 tramos de línea A
    assert(cerca(costoRuta(g, "universidad", "poblado"), 11.1));     // 8 tramos de línea A
    assert(cerca(costoRuta(g, "san_javier", "santo_domingo"), 35.5));  // B + A + K y 2 transbordos
    std::cout << "OK rutas conocidas\n";

    // 3. El sentido contrario cuesta lo mismo (las aristas son simétricas).
    assert(cerca(costoRuta(g, "la_estrella", "niquia"), 40.2));
    std::cout << "OK simetría\n";

    // 4. A* y Dijkstra coinciden en costo en 20 pares aleatorios.
    std::mt19937 generador(2026);
    std::uniform_int_distribution<int> dist(0, static_cast<int>(g.estaciones.size()) - 1);
    for (int i = 0; i < 20; ++i) {
        int a = dist(generador);
        int b = dist(generador);
        if (a == b) b = (b + 7) % static_cast<int>(g.estaciones.size());
        const Resultado d = dijkstra(g, g.nodosDeEstacion(a), g.nodosDeEstacion(b));
        const Resultado s = astar(g, g.nodosDeEstacion(a), g.nodosDeEstacion(b));
        assert(d.encontrado == s.encontrado);
        if (d.encontrado) {
            assert(cerca(d.costo, s.costo));
            assert(!s.nodos.empty());
        }
    }
    std::cout << "OK A* = Dijkstra en 20 pares\n";

    // 5. Yen sobre la red real: es un árbol, así que solo hay una ruta simple.
    const Resultado directa = dijkstra(g, g.nodosDeEstacion(estacion(g, "niquia")),
                                       g.nodosDeEstacion(estacion(g, "arvi")));
    const std::vector<Resultado> enRed =
        yen(g, directa.nodos.front(), directa.nodos.back(), 3);
    assert(enRed.size() == 1);
    assert(cerca(enRed.front().costo, directa.costo));

    // 5b. Yen sobre un grafo de prueba con ciclo: A-B-C-D y A-E-C forman dos
    // caminos de 3 minutos entre A y D, y A-D directo cuesta 4.
    Graph s;
    s.lineas.push_back({"X", "#000000"});
    const char* ids[6] = {"a", "b", "c", "d", "e", "f"};
    for (int i = 0; i < 6; ++i) {
        s.estaciones.push_back({ids[i], ids[i], 0.0, 0.0, {0}});
        s.nodos.push_back({i, 0});
        s.adj.emplace_back();
    }
    const auto unir = [&](int a, int b, double m) {
        s.adj[a].push_back({b, m, false});
        s.adj[b].push_back({a, m, false});
    };
    unir(0, 1, 1.0);  // a-b
    unir(1, 2, 1.0);  // b-c
    unir(2, 3, 1.0);  // c-d
    unir(0, 3, 4.0);  // a-d
    unir(0, 4, 1.0);  // a-e
    unir(4, 2, 1.0);  // e-c
    const std::vector<Resultado> sinteticas = yen(s, 0, 3, 3);
    assert(sinteticas.size() == 3);
    assert(cerca(sinteticas[0].costo, 3.0));
    assert(cerca(sinteticas[1].costo, 3.0));
    assert(cerca(sinteticas[2].costo, 4.0));
    for (std::size_t i = 1; i < sinteticas.size(); ++i) {
        assert(sinteticas[i].costo >= sinteticas[i - 1].costo - 1e-9);
        assert(sinteticas[i].nodos != sinteticas[i - 1].nodos);
    }
    std::cout << "OK rutas alternativas (1 en la red, 3 en el grafo con ciclo)\n";

    // 6. Puntos de articulación: Acevedo y Santo Domingo parten la red;
    //    una estación terminal como La Estrella no.
    const std::vector<int> corte = puntosDeArticulacion(g);
    const auto esCorte = [&](const std::string& id) {
        return std::find(corte.begin(), corte.end(), estacion(g, id)) != corte.end();
    };
    assert(esCorte("acevedo"));
    assert(esCorte("santo_domingo"));
    assert(!esCorte("la_estrella"));
    assert(!esCorte("niquia"));
    std::cout << "OK puntos de articulación (" << corte.size() << ")\n";

    // 7. Estación cerrada: sin Santo Domingo no se llega a Arví.
    Bloqueo bloqueo(static_cast<int>(g.nodos.size()));
    for (int n : g.nodosDeEstacion(estacion(g, "santo_domingo"))) bloqueo.nodos[n] = 1;
    const Resultado cerrada = dijkstra(g, g.nodosDeEstacion(estacion(g, "niquia")),
                                       g.nodosDeEstacion(estacion(g, "arvi")), &bloqueo);
    assert(!cerrada.encontrado);
    std::cout << "OK estación cerrada\n";

    // 8. El transbordo cuesta exactamente lo configurado.
    Graph sinCosto;
    assert(cargarGrafo(CARPETA_DATOS, 0.0, sinCosto, false));
    assert(cerca(costoRuta(sinCosto, "san_javier", "santo_domingo"), 29.5));
    std::cout << "OK penalización de transbordo\n";

    // 9. Trie: prefijos sin tildes ni mayúsculas.
    Trie trie;
    for (std::size_t i = 0; i < g.estaciones.size(); ++i) {
        trie.insertar(g.estaciones[i].nombre, static_cast<int>(i));
    }
    const std::vector<int> conSan = trie.porPrefijo("SAN", 10);
    assert(conSan.size() >= 4);  // San Antonio, San Javier, San José, Santa Lucía
    const std::vector<int> itagui = trie.porPrefijo("itagui", 10);
    assert(itagui.size() == 1 && g.estaciones[itagui[0]].id == "itagui");
    const std::vector<int> arvi = trie.porPrefijo("árv", 10);
    assert(arvi.size() == 1 && g.estaciones[arvi[0]].id == "arvi");
    assert(trie.porPrefijo("zzz", 10).empty());
    std::cout << "OK autocompletado con Trie\n";

    // 10. Rutas alimentadoras: el grafo crece, aparecen alternativas y cerrar
    // una estación de paso deja de ser un callejón sin salida.
    Graph conBus;
    assert(cargarGrafo(CARPETA_DATOS, 3.0, conBus, true));
    int paradas = 0;
    for (const Estacion& e : conBus.estaciones) {
        if (e.bus) ++paradas;
    }
    assert(paradas == 11879);
    assert(conBus.lineas.size() == 264);
    assert(conBus.nodos.size() == 11932);
    assert(conBus.totalAristas() == 30610);

    const Resultado baseBus = dijkstra(conBus, conBus.nodosDeEstacion(estacion(conBus, "san_antonio")),
                                       conBus.nodosDeEstacion(estacion(conBus, "arvi")));
    assert(baseBus.encontrado);
    const std::vector<Resultado> alternativas =
        yen(conBus, baseBus.nodos.front(), baseBus.nodos.back(), 3);
    assert(alternativas.size() == 3);
    assert(alternativas[0].costo < 47.0);  // el bus mejora el tiempo del Metro solo

    Bloqueo cierreBus(static_cast<int>(conBus.nodos.size()));
    for (int n : conBus.nodosDeEstacion(estacion(conBus, "universidad"))) {
        cierreBus.nodos[n] = 1;
    }
    const Resultado desvio = dijkstra(conBus, conBus.nodosDeEstacion(estacion(conBus, "san_antonio")),
                                      conBus.nodosDeEstacion(estacion(conBus, "acevedo")), &cierreBus);
    assert(desvio.encontrado);

    // Con las rutas urbanas, cerrar San Antonio ya no aísla el occidente:
    // Floresta -> Poblado encuentra desvío (baja en Cisneros y toma la A).
    Bloqueo cierreSanAntonio(static_cast<int>(conBus.nodos.size()));
    for (int n : conBus.nodosDeEstacion(estacion(conBus, "san_antonio"))) {
        cierreSanAntonio.nodos[n] = 1;
    }
    const Resultado cruce = dijkstra(conBus, conBus.nodosDeEstacion(estacion(conBus, "floresta")),
                                     conBus.nodosDeEstacion(estacion(conBus, "poblado")),
                                     &cierreSanAntonio);
    assert(cruce.encontrado);
    std::cout << "OK rutas de bus (" << paradas << " paradas, " << alternativas.size()
              << " alternativas, desvio por " << desvio.costo << " min, Floresta->Poblado con"
              << " San Antonio cerrada: " << cruce.costo << " min)\n";

    std::cout << "Todas las pruebas pasaron\n";
    return 0;
}
