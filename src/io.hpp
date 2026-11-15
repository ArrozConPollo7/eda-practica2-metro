// io.hpp - lectura del CSV y exportación a DOT, JSON y SVG.
#pragma once

#include <string>

#include "algorithms.hpp"
#include "graph.hpp"

// Lee data/stations.csv y data/edges.csv y construye el grafo expandido
// (estación, línea). "transbordo" es la penalización en minutos.
bool cargarGrafo(const std::string& dirDatos, double transbordo, Graph& g);

// Cada exportación recibe la ruta seleccionada (nullptr si no hay ninguna).
bool exportarDot(const std::string& ruta, const Graph& g, const Resultado* seleccion);
bool exportarJson(const std::string& ruta, const Graph& g, const Resultado* seleccion);
bool exportarSvg(const std::string& ruta, const Graph& g, const Resultado* seleccion);
