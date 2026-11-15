// algorithms.hpp - búsquedas de caminos sobre el grafo.
#pragma once

#include <vector>

#include "graph.hpp"

// Resultado de una búsqueda: camino, costo, trabajo realizado y tiempo.
struct Resultado {
    std::vector<int> nodos;   // camino en el grafo expandido (estación, línea)
    double costo = 0.0;       // minutos
    long explorados = 0;      // nodos extraídos del heap
    double ms = 0.0;          // duración de la búsqueda en milisegundos
    bool encontrado = false;
};

// Nodos y aristas prohibidos; lo usan Yen y la simulación de estación cerrada.
struct Bloqueo {
    std::vector<char> nodos;
    std::vector<std::vector<char>> aristas;
    explicit Bloqueo(int n) : nodos(n, 0), aristas(n, std::vector<char>(n, 0)) {}
};

// Dijkstra con priority_queue entre conjuntos de nodos: el heap arranca con
// todos los nodos del origen y la búsqueda termina al sacar uno del destino.
// Complejidad O((V + E) log V).
Resultado dijkstra(const Graph& g, const std::vector<int>& origenes,
                   const std::vector<int>& destinos, const Bloqueo* bloqueo = nullptr);
Resultado dijkstra(const Graph& g, int origen, int destino, const Bloqueo* bloqueo = nullptr);

// A* con heurística haversine / velocidad máxima: mismo costo, menos nodos.
Resultado astar(const Graph& g, const std::vector<int>& origenes,
                const std::vector<int>& destinos, const Bloqueo* bloqueo = nullptr);
Resultado astar(const Graph& g, int origen, int destino, const Bloqueo* bloqueo = nullptr);

// k caminos más cortos sin ciclos (algoritmo de Yen).
std::vector<Resultado> yen(const Graph& g, int origen, int destino, int k);

// Estaciones cuyo cierre desconecta la red (DFS con disc/low).
std::vector<int> puntosDeArticulacion(const Graph& g);

// Suma de minutos de un camino ya calculado (-1 si falta alguna arista).
double costoDeCamino(const Graph& g, const std::vector<int>& camino);
