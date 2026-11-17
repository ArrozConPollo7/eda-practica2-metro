// servidor.hpp - interfaz web del navegador (servidor HTTP mínimo con sockets).
#pragma once

#include <string>

// Levanta un servidor HTTP en el puerto indicado, sirve web/index.html y la API
// JSON. Bloquea hasta que se corte con Ctrl+C. Devuelve el código de salida.
int iniciarServidor(const std::string& carpetaDatos, double transbordo, int puerto,
                    bool conBuses = true);
