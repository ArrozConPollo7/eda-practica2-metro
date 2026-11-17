// algorithms.cpp - Dijkstra, A*, Yen y puntos de articulación.
#include "algorithms.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <limits>
#include <queue>
#include <set>
#include <utility>

namespace {

using Reloj = std::chrono::high_resolution_clock;
using Par = std::pair<double, int>;  // (costo, nodo)

constexpr double INFINITO = std::numeric_limits<double>::infinity();

double milisegundosDesde(const Reloj::time_point& inicio) {
    const auto fin = Reloj::now();
    return std::chrono::duration<double, std::milli>(fin - inicio).count();
}

// Arma el camino siguiendo los predecesores hasta el nodo inicial.
std::vector<int> reconstruir(const std::vector<int>& previo, int destino) {
    std::vector<int> camino;
    for (int v = destino; v != -1; v = previo[v]) camino.push_back(v);
    std::reverse(camino.begin(), camino.end());
    return camino;
}

bool bloqueado(const Bloqueo* b, int u, int v) {
    if (b == nullptr) return false;
    return b->nodos[v] != 0 || b->aristas[u][v] != 0;
}

}  // namespace

double costoDeCamino(const Graph& g, const std::vector<int>& camino) {
    double total = 0.0;
    for (std::size_t i = 0; i + 1 < camino.size(); ++i) {
        double mejor = -1.0;
        for (const Arista& a : g.adj[camino[i]]) {
            if (a.destino != camino[i + 1]) continue;
            if (mejor < 0.0 || a.minutos < mejor) mejor = a.minutos;
        }
        if (mejor < 0.0) return -1.0;
        total += mejor;
    }
    return total;
}

Resultado dijkstra(const Graph& g, const std::vector<int>& origenes,
                   const std::vector<int>& destinos, const Bloqueo* bloqueo) {
    const auto inicio = Reloj::now();
    Resultado r;
    const int n = static_cast<int>(g.adj.size());
    if (origenes.empty() || destinos.empty()) {
        r.ms = milisegundosDesde(inicio);
        return r;
    }

    // dist[v] = menor tiempo conocido desde el origen; el heap guarda los
    // candidatos y se descartan las entradas obsoletas.
    std::vector<double> dist(n, INFINITO);
    std::vector<int> previo(n, -1);
    std::vector<char> esDestino(n, 0);
    std::priority_queue<Par, std::vector<Par>, std::greater<Par>> heap;

    for (int d : destinos) {
        if (d >= 0 && d < n) esDestino[d] = 1;
    }
    for (int o : origenes) {
        if (o < 0 || o >= n) continue;
        if (bloqueo != nullptr && bloqueo->nodos[o] != 0) continue;
        dist[o] = 0.0;
        heap.push({0.0, o});
    }

    int llegada = -1;
    while (!heap.empty()) {
        const Par actual = heap.top();
        heap.pop();
        const int u = actual.second;
        if (actual.first > dist[u]) continue;  // entrada obsoleta
        ++r.explorados;
        if (esDestino[u] != 0) {
            llegada = u;
            break;
        }
        for (const Arista& a : g.adj[u]) {
            if (bloqueado(bloqueo, u, a.destino)) continue;
            const double nuevo = dist[u] + a.minutos;
            if (nuevo < dist[a.destino]) {
                dist[a.destino] = nuevo;
                previo[a.destino] = u;
                heap.push({nuevo, a.destino});
            }
        }
    }

    if (llegada >= 0) {
        r.encontrado = true;
        r.costo = dist[llegada];
        r.nodos = reconstruir(previo, llegada);
    }
    r.ms = milisegundosDesde(inicio);
    return r;
}

Resultado dijkstra(const Graph& g, int origen, int destino, const Bloqueo* bloqueo) {
    return dijkstra(g, std::vector<int>{origen}, std::vector<int>{destino}, bloqueo);
}

Resultado astar(const Graph& g, const std::vector<int>& origenes,
                const std::vector<int>& destinos, const Bloqueo* bloqueo) {
    const auto inicio = Reloj::now();
    Resultado r;
    const int n = static_cast<int>(g.adj.size());
    if (origenes.empty() || destinos.empty()) {
        r.ms = milisegundosDesde(inicio);
        return r;
    }

    // h(v) = distancia en línea recta hasta el destino más cercano dividida por
    // la velocidad máxima de la red. Ningún tramo recorre esa distancia más
    // rápido, así que h nunca sobreestima lo que falta (heurística admisible).
    const double vMax = g.velocidadMaxima();
    std::vector<char> esDestino(n, 0);
    std::vector<int> estacionesDestino;
    for (int d : destinos) {
        if (d < 0 || d >= n) continue;
        esDestino[d] = 1;
        estacionesDestino.push_back(g.nodos[d].estacion);
    }
    // La heurística se calcula una sola vez por nodo: son 2 senos y 2 cosenos,
    // mucho más caros que revisar un vecino.
    std::vector<double> heuristica(n, 0.0);
    for (int v = 0; v < n; ++v) {
        double mejor = INFINITO;
        const int est = g.nodos[v].estacion;
        for (int destino : estacionesDestino) {
            const double d = g.distanciaKm(est, destino);
            if (d < mejor) mejor = d;
        }
        heuristica[v] = mejor / vMax;
    }
    const auto h = [&](int v) { return heuristica[v]; };

    std::vector<double> dist(n, INFINITO);
    std::vector<int> previo(n, -1);
    std::vector<char> cerrado(n, 0);
    std::priority_queue<Par, std::vector<Par>, std::greater<Par>> heap;

    for (int o : origenes) {
        if (o < 0 || o >= n) continue;
        if (bloqueo != nullptr && bloqueo->nodos[o] != 0) continue;
        dist[o] = 0.0;
        heap.push({h(o), o});
    }

    int llegada = -1;
    while (!heap.empty()) {
        const int u = heap.top().second;
        heap.pop();
        if (cerrado[u] != 0) continue;
        cerrado[u] = 1;
        ++r.explorados;
        if (esDestino[u] != 0) {
            llegada = u;
            break;
        }
        for (const Arista& a : g.adj[u]) {
            if (bloqueado(bloqueo, u, a.destino)) continue;
            const double nuevo = dist[u] + a.minutos;
            if (nuevo < dist[a.destino]) {
                dist[a.destino] = nuevo;
                previo[a.destino] = u;
                heap.push({nuevo + h(a.destino), a.destino});
            }
        }
    }

    if (llegada >= 0) {
        r.encontrado = true;
        r.costo = dist[llegada];
        r.nodos = reconstruir(previo, llegada);
    }
    r.ms = milisegundosDesde(inicio);
    return r;
}

Resultado astar(const Graph& g, int origen, int destino, const Bloqueo* bloqueo) {
    return astar(g, std::vector<int>{origen}, std::vector<int>{destino}, bloqueo);
}

namespace {

// Firma de una ruta para comparar alternativas: estaciones del Metro visitadas
// más los buses usados. Ignora las paradas que solo se tocan al entrar o salir
// del bus, para que un rodeo peatonal no cuente como ruta distinta.
std::vector<int> firmaRuta(const Graph& g, const std::vector<int>& camino) {
    std::vector<int> firma;
    for (int est : estacionesDeRuta(g, camino)) {
        if (!g.estaciones[est].bus) firma.push_back(est);
    }
    std::vector<char> usadas(g.lineas.size(), 0);
    for (int n : camino) {
        if (!g.estaciones[g.nodos[n].estacion].bus) continue;
        const int linea = g.nodos[n].linea;
        if (linea >= 0 && static_cast<std::size_t>(linea) < usadas.size() && usadas[linea] == 0) {
            usadas[linea] = 1;
            firma.push_back(100000 + linea);
        }
    }
    return firma;
}

}  // namespace

std::vector<Resultado> yen(const Graph& g, int origen, int destino, int k) {
    std::vector<Resultado> aceptadas;
    if (k <= 0) return aceptadas;

    const Resultado primera = dijkstra(g, origen, destino);
    if (!primera.encontrado) return aceptadas;
    aceptadas.push_back(primera);

    const int n = static_cast<int>(g.adj.size());
    std::set<std::pair<double, std::vector<int>>> candidatos;

    while (static_cast<int>(aceptadas.size()) < k) {
        const std::vector<int> anterior = aceptadas.back().nodos;

        // Se corta la última ruta aceptada en cada uno de sus nodos y se busca
        // un desvío mínimo que no repita las aristas de las rutas aceptadas.
        for (std::size_t i = 0; i + 1 < anterior.size(); ++i) {
            const std::vector<int> raiz(anterior.begin(),
                                        anterior.begin() + static_cast<long>(i) + 1);
            Bloqueo bloqueo(n);
            for (const Resultado& p : aceptadas) {
                if (p.nodos.size() <= i + 1) continue;
                if (std::equal(raiz.begin(), raiz.end(), p.nodos.begin())) {
                    bloqueo.aristas[p.nodos[i]][p.nodos[i + 1]] = 1;
                }
            }
            for (std::size_t j = 0; j + 1 < raiz.size(); ++j) {
                bloqueo.nodos[raiz[j]] = 1;  // la raíz no se repite (sin ciclos)
            }

            const Resultado cola = dijkstra(g, anterior[i], destino, &bloqueo);
            if (!cola.encontrado) continue;

            std::vector<int> total(raiz.begin(), raiz.end() - 1);
            total.insert(total.end(), cola.nodos.begin(), cola.nodos.end());
            const double costo = costoDeCamino(g, total);
            if (costo < 0.0) continue;

            // Dos caminos que pasan por las mismas estaciones y usan los mismos
            // buses no son rutas alternativas.
            const std::vector<int> firma = firmaRuta(g, total);
            bool repetida = false;
            for (const Resultado& p : aceptadas) {
                if (firmaRuta(g, p.nodos) == firma) {
                    repetida = true;
                    break;
                }
            }
            if (repetida) continue;
            candidatos.insert({costo, total});
        }

        if (candidatos.empty()) break;

        bool elegida = false;
        for (auto it = candidatos.begin(); it != candidatos.end(); ++it) {
            bool repetida = false;
            const std::vector<int> firma = firmaRuta(g, it->second);
            for (const Resultado& p : aceptadas) {
                if (p.nodos == it->second || firmaRuta(g, p.nodos) == firma) {
                    repetida = true;
                    break;
                }
            }
            if (repetida) continue;
            Resultado r;
            r.nodos = it->second;
            r.costo = it->first;
            r.encontrado = true;
            aceptadas.push_back(r);
            candidatos.erase(it);
            elegida = true;
            break;
        }
        if (!elegida) break;
    }
    return aceptadas;
}

std::vector<int> puntosDeArticulacion(const Graph& g) {
    const int n = static_cast<int>(g.estaciones.size());

    // Se colapsa el grafo expandido a estaciones: dos estaciones son vecinas si
    // hay un tramo entre ellas (los transbordos no cuentan como arista). Se usa
    // un conjunto de pares y no una matriz n x n: con los buses urbanos hay
    // ~12.000 estaciones y la matriz pedía 140 MB.
    std::vector<std::vector<int>> vecinos(n);
    std::set<std::pair<int, int>> visto;
    for (std::size_t u = 0; u < g.nodos.size(); ++u) {
        const int a = g.nodos[u].estacion;
        for (const Arista& e : g.adj[u]) {
            const int b = g.nodos[e.destino].estacion;
            // Los transbordos dentro de la misma estación no cuentan como
            // arista; los accesos a pie entre parada y estación sí, porque
            // conectan dos puntos distintos del sistema.
            if (a == b) continue;
            if (!visto.insert({std::min(a, b), std::max(a, b)}).second) continue;
            vecinos[a].push_back(b);
            vecinos[b].push_back(a);
        }
    }

    // DFS con disc/low: un nodo es de articulación si algún hijo suyo no
    // alcanza un ancestro suyo por otro camino (low[hijo] >= disc[nodo]).
    std::vector<int> disc(n, 0);
    std::vector<int> low(n, 0);
    std::vector<int> padre(n, -1);
    std::vector<int> articulacion(n, 0);
    int tiempo = 0;

    std::function<void(int)> dfs = [&](int u) {
        disc[u] = low[u] = ++tiempo;
        int hijos = 0;
        for (int v : vecinos[u]) {
            if (disc[v] == 0) {
                padre[v] = u;
                ++hijos;
                dfs(v);
                low[u] = std::min(low[u], low[v]);
                if (padre[u] != -1 && low[v] >= disc[u]) articulacion[u] = 1;
            } else if (v != padre[u]) {
                low[u] = std::min(low[u], disc[v]);
            }
        }
        if (padre[u] == -1 && hijos > 1) articulacion[u] = 1;
    };

    for (int v = 0; v < n; ++v) {
        if (disc[v] == 0) dfs(v);
    }

    std::vector<int> salida;
    for (int v = 0; v < n; ++v) {
        if (articulacion[v] == 0) continue;
        // Las paradas de una ruta alimentadora son casi todas hojas: como punto
        // crítico del sistema no aportan y llenarían el mapa de ruido.
        if (g.estaciones[static_cast<std::size_t>(v)].bus) continue;
        salida.push_back(v);
    }
    return salida;
}
