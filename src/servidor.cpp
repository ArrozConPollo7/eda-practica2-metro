// servidor.cpp - servidor HTTP mínimo (solo GET) para la interfaz web.
//
// Se usan sockets POSIX de la biblioteca del sistema: no hay dependencias
// externas. El servidor es de un solo hilo y atiende una petición a la vez, que
// es suficiente para un navegador y una persona.
#include "servidor.hpp"

// Sockets: POSIX en Linux/macOS y Winsock en Windows.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "algorithms.hpp"
#include "graph.hpp"
#include "io.hpp"
#include "trie.hpp"

#ifdef _WIN32
using socket_t = SOCKET;
static void cerrarSocket(socket_t s) { closesocket(s); }
static bool iniciarSockets() { WSADATA d; return WSAStartup(MAKEWORD(2, 2), &d) == 0; }
static void terminarSockets() { WSACleanup(); }
static bool socketValido(socket_t s) { return s != INVALID_SOCKET; }
#else
using socket_t = int;
static void cerrarSocket(int s) { ::close(s); }
static bool iniciarSockets() { return true; }
static void terminarSockets() {}
static bool socketValido(int s) { return s >= 0; }
#endif

namespace {

struct Estado {
    std::string carpeta;
    Graph grafo;
    Trie trie;
    double transbordo = 3.0;
    bool conBuses = true;
    std::string jsonGrafo;  // se arma una vez: las estaciones no cambian
};

std::string formato(double valor, int decimales = 1) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(decimales) << valor;
    return os.str();
}

std::string escapar(const std::string& texto) {
    std::string salida;
    for (unsigned char c : texto) {
        if (c == '"') salida += "\\\"";
        else if (c == '\\') salida += "\\\\";
        else if (c == '\n') salida += "\\n";
        else if (c == '\t') salida += "\\t";
        // Los controles no son válidos dentro de una cadena JSON.
        else if (c >= 0x20) salida.push_back(static_cast<char>(c));
    }
    return salida;
}

// Decodifica %XX y '+' de una cadena de consulta o de una ruta.
std::string decodificar(const std::string& texto) {
    std::string salida;
    for (std::size_t i = 0; i < texto.size(); ++i) {
        if (texto[i] == '+') {
            salida.push_back(' ');
        } else if (texto[i] == '%' && i + 2 < texto.size()) {
            const std::string hex = texto.substr(i + 1, 2);
            salida.push_back(static_cast<char>(std::strtol(hex.c_str(), nullptr, 16)));
            i += 2;
        } else {
            salida.push_back(texto[i]);
        }
    }
    return salida;
}

std::vector<std::pair<std::string, std::string>> parametros(const std::string& consulta) {
    std::vector<std::pair<std::string, std::string>> salida;
    std::size_t inicio = 0;
    while (inicio <= consulta.size()) {
        const std::size_t fin = consulta.find('&', inicio);
        const std::string trozo = consulta.substr(inicio, fin == std::string::npos
                                                               ? std::string::npos
                                                               : fin - inicio);
        const std::size_t igual = trozo.find('=');
        if (igual != std::string::npos) {
            salida.push_back({decodificar(trozo.substr(0, igual)),
                              decodificar(trozo.substr(igual + 1))});
        } else if (!trozo.empty()) {
            salida.push_back({decodificar(trozo), ""});
        }
        if (fin == std::string::npos) break;
        inicio = fin + 1;
    }
    return salida;
}

std::string valorDe(const std::vector<std::pair<std::string, std::string>>& params,
                    const std::string& clave, const std::string& porDefecto = "") {
    for (const auto& p : params) {
        if (p.first == clave) return p.second;
    }
    return porDefecto;
}

std::string listaLineas(const Graph& g, int estacion) {
    std::string salida = "[";
    for (std::size_t i = 0; i < g.estaciones[estacion].lineas.size(); ++i) {
        if (i > 0) salida += ",";
        salida += "\"" + g.lineas[g.estaciones[estacion].lineas[i]].codigo + "\"";
    }
    return salida + "]";
}

std::string jsonEstacion(const Graph& g, int i) {
    const Estacion& e = g.estaciones[i];
    std::ostringstream os;
    os << "{\"id\":\"" << e.id << "\",\"nombre\":\"" << escapar(e.nombre) << "\",\"lat\":"
       << formato(e.lat, 5) << ",\"lon\":" << formato(e.lon, 5) << ",\"lineas\":"
       << listaLineas(g, i) << "}";
    return os.str();
}

std::string jsonGrafoCompleto(const Graph& g) {
    std::ostringstream os;
    os << "{\"lineas\":[";
    for (std::size_t i = 0; i < g.lineas.size(); ++i) {
        if (i > 0) os << ",";
        os << "{\"codigo\":\"" << g.lineas[i].codigo << "\",\"color\":\"" << g.lineas[i].color
           << "\"}";
    }
    // Las estaciones del Metro y las paradas de bus van en listas separadas: el
    // mapa dibuja las primeras siempre y las paradas solo si se piden.
    os << "],\"estaciones\":[";
    bool primeroEstacion = true;
    for (std::size_t i = 0; i < g.estaciones.size(); ++i) {
        if (g.estaciones[i].bus) continue;
        if (!primeroEstacion) os << ",";
        primeroEstacion = false;
        os << jsonEstacion(g, static_cast<int>(i));
    }
    os << "],\"paradas\":[";
    bool primeraParada = true;
    for (std::size_t i = 0; i < g.estaciones.size(); ++i) {
        const Estacion& e = g.estaciones[i];
        if (!e.bus) continue;
        if (!primeraParada) os << ",";
        primeraParada = false;
        os << "{\"id\":\"" << e.id << "\",\"nombre\":\"" << escapar(e.nombre)
           << "\",\"lat\":" << formato(e.lat, 6) << ",\"lon\":" << formato(e.lon, 6)
           << ",\"ruta\":\"" << g.lineas[static_cast<std::size_t>(e.lineas.front())].codigo
           << "\"}";
    }
    os << "],\"aristas\":[";
    std::vector<std::pair<int, int>> vistas;
    bool primero = true;
    for (std::size_t u = 0; u < g.nodos.size(); ++u) {
        if (g.estaciones[static_cast<std::size_t>(g.nodos[u].estacion)].bus) continue;
        for (const Arista& a : g.adj[u]) {
            if (a.transbordo) continue;
            const int estA = g.nodos[u].estacion;
            const int estB = g.nodos[a.destino].estacion;
            if (g.estaciones[static_cast<std::size_t>(estB)].bus) continue;
            const std::pair<int, int> clave{std::min(estA, estB), std::max(estA, estB)};
            if (std::find(vistas.begin(), vistas.end(), clave) != vistas.end()) continue;
            vistas.push_back(clave);
            if (!primero) os << ",";
            primero = false;
            os << "{\"o\":\"" << g.estaciones[clave.first].id << "\",\"d\":\""
               << g.estaciones[clave.second].id << "\",\"m\":" << formato(a.minutos)
               << ",\"l\":\"" << g.lineas[g.nodos[u].linea].codigo << "\"}";
        }
    }
    os << "]}";
    return os.str();
}

// Una ruta como objeto JSON, con las paradas y su tiempo acumulado.
std::string jsonRuta(const Graph& g, const Resultado& r) {
    const std::vector<Tramo> tramos = tramosDeRuta(g, r.nodos);
    std::ostringstream os;
    os << "{\"minutos\":" << formato(r.costo) << ",\"paradas\":[";
    if (!tramos.empty()) {
        const int origen = g.nodos[r.nodos.front()].estacion;
        int lineaActual = tramos.front().linea;
        double acumulado = 0.0;
        os << "{\"id\":\"" << g.estaciones[origen].id << "\",\"nombre\":\""
           << escapar(g.estaciones[origen].nombre) << "\",\"linea\":\""
           << g.lineas[lineaActual].codigo << "\",\"acum\":0.0}";
        for (const Tramo& t : tramos) {
            acumulado += t.minutos;  // el transbordo también suma tiempo
            if (t.transbordo) {
                lineaActual = t.linea;
                continue;
            }
            os << ",{\"id\":\"" << g.estaciones[t.hasta].id << "\",\"nombre\":\""
               << escapar(g.estaciones[t.hasta].nombre) << "\",\"linea\":\""
               << g.lineas[t.linea].codigo << "\",\"acum\":" << formato(acumulado) << "}";
        }
    }
    os << "],\"tramos\":[";
    for (std::size_t i = 0; i < tramos.size(); ++i) {
        if (i > 0) os << ",";
        os << "{\"desde\":\"" << g.estaciones[tramos[i].desde].id << "\",\"hasta\":\""
           << g.estaciones[tramos[i].hasta].id << "\",\"minutos\":"
           << formato(tramos[i].minutos) << ",\"linea\":\"" << g.lineas[tramos[i].linea].codigo
           << "\",\"transbordo\":" << (tramos[i].transbordo ? "true" : "false") << "}";
    }
    os << "]}";
    return os.str();
}


std::string leerArchivo(const std::string& ruta) {
    std::ifstream archivo(ruta, std::ios::binary);
    if (!archivo) return "";
    std::ostringstream os;
    os << archivo.rdbuf();
    return os.str();
}

void responder(socket_t cliente, const std::string& estado, const std::string& tipo,
               const std::string& cuerpo) {
    std::ostringstream os;
    os << "HTTP/1.1 " << estado << "\r\n"
       << "Content-Type: " << tipo << "\r\n"
       << "Content-Length: " << cuerpo.size() << "\r\n"
       << "Cache-Control: no-store\r\n"
       << "Connection: close\r\n\r\n"
       << cuerpo;
    const std::string datos = os.str();
    std::size_t enviado = 0;
    while (enviado < datos.size()) {
        const auto n = ::send(cliente, datos.data() + enviado, datos.size() - enviado, 0);
        if (n <= 0) break;
        enviado += static_cast<std::size_t>(n);
    }
}

// Vuelve a construir el grafo solo si cambió la penalización de transbordo.
void asegurarTransbordo(Estado& e, double pedido) {
    if (std::fabs(pedido - e.transbordo) < 1e-9) return;
    Graph nuevo;
    if (cargarGrafo(e.carpeta, pedido, nuevo, e.conBuses)) {
        e.grafo = std::move(nuevo);
        e.transbordo = pedido;
    }
}

std::string manejarApi(Estado& e, const std::string& ruta,
                       const std::vector<std::pair<std::string, std::string>>& params) {
    const Graph& g = e.grafo;

    if (ruta == "/api/grafo") return e.jsonGrafo;

    if (ruta == "/api/estaciones") {
        const std::string q = valorDe(params, "q");
        std::string salida = "{\"estaciones\":[";
        if (!q.empty()) {
            const std::vector<int> coincidencias = e.trie.porPrefijo(q, 8);
            for (std::size_t i = 0; i < coincidencias.size(); ++i) {
                if (i > 0) salida += ",";
                salida += jsonEstacion(g, coincidencias[i]);
            }
        }
        return salida + "]}";
    }

    if (ruta == "/api/articulacion") {
        const std::vector<int> puntos = puntosDeArticulacion(g);
        std::string salida = "{\"puntos\":[";
        for (std::size_t i = 0; i < puntos.size(); ++i) {
            if (i > 0) salida += ",";
            salida += jsonEstacion(g, puntos[i]);
        }
        return salida + "]}";
    }

    if (ruta == "/api/ruta") {
        const int origen = g.indiceEstacion(valorDe(params, "o"));
        const int destino = g.indiceEstacion(valorDe(params, "d"));
        double penalizacion = 3.0;
        const std::string textoPenalizacion = valorDe(params, "transbordo", "3");
        try {
            penalizacion = std::stod(textoPenalizacion);
        } catch (...) {
            penalizacion = 3.0;
        }
        asegurarTransbordo(e, penalizacion);
        const Graph& gg = e.grafo;

        if (origen < 0 || destino < 0) {
            return "{\"encontrada\":false,\"error\":\"estación desconocida\"}";
        }

        Bloqueo bloqueo(static_cast<int>(gg.nodos.size()));
        const int cerrada = gg.indiceEstacion(valorDe(params, "cerrada"));
        if (cerrada >= 0) {
            for (int n : gg.nodosDeEstacion(cerrada)) bloqueo.nodos[n] = 1;
        }

        const std::vector<int> origenes = gg.nodosDeEstacion(origen);
        const std::vector<int> destinos = gg.nodosDeEstacion(destino);
        const Resultado d = dijkstra(gg, origenes, destinos, &bloqueo);
        const Resultado a = astar(gg, origenes, destinos, &bloqueo);

        std::ostringstream os;
        os << "{\"encontrada\":" << (d.encontrado ? "true" : "false");
        os << ",\"rutas\":[";
        if (d.encontrado) {
            int k = 1;
            try {
                k = std::stoi(valorDe(params, "k", "1"));
            } catch (...) {
                k = 1;
            }
            k = std::max(1, std::min(k, 6));

            std::vector<Resultado> rutas;
            if (k == 1 || cerrada >= 0) {
                rutas.push_back(d);
            } else {
                rutas = yen(gg, d.nodos.front(), d.nodos.back(), k);
            }
            for (std::size_t i = 0; i < rutas.size(); ++i) {
                if (i > 0) os << ",";
                os << jsonRuta(gg, rutas[i]);
            }
        }
        os << "],\"dijkstra\":{\"costo\":" << formato(d.costo) << ",\"nodos\":" << d.explorados
           << ",\"us\":" << formato(d.ms * 1000.0, 1)
           << "},\"astar\":{\"costo\":" << formato(a.costo) << ",\"nodos\":" << a.explorados
           << ",\"us\":" << formato(a.ms * 1000.0, 1) << "}}";
        return os.str();
    }

    return "{\"error\":\"ruta desconocida\"}";
}

}  // namespace

int iniciarServidor(const std::string& carpetaDatos, double transbordo, int puerto, bool conBuses) {
    Estado estado;
    estado.carpeta = carpetaDatos;
    estado.transbordo = transbordo;
    estado.conBuses = conBuses;
    if (!cargarGrafo(carpetaDatos, transbordo, estado.grafo, conBuses)) {
        std::cerr << "No se pudieron cargar los datos de " << carpetaDatos << "\n";
        return 1;
    }
    for (std::size_t i = 0; i < estado.grafo.estaciones.size(); ++i) {
        estado.trie.insertar(estado.grafo.estaciones[i].nombre, static_cast<int>(i));
        estado.trie.insertar(estado.grafo.estaciones[i].id, static_cast<int>(i));
    }
    estado.jsonGrafo = jsonGrafoCompleto(estado.grafo);

    if (!iniciarSockets()) {
        std::cerr << "No se pudo iniciar Winsock\n";
        return 1;
    }
    const socket_t servidor = ::socket(AF_INET, SOCK_STREAM, 0);
    if (!socketValido(servidor)) {
        std::cerr << "No se pudo crear el socket\n";
        return 1;
    }
    int uno = 1;
#ifdef _WIN32
    ::setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&uno), sizeof(uno));
#else
    ::setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof(uno));
#endif

    sockaddr_in direccion{};
    direccion.sin_family = AF_INET;
    // Loopback a propósito: la interfaz es local y así Windows no pide permiso
    // al Firewall por exponer el puerto a la red.
    direccion.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    direccion.sin_port = htons(static_cast<uint16_t>(puerto));
    if (::bind(servidor, reinterpret_cast<sockaddr*>(&direccion),
               static_cast<int>(sizeof(direccion))) < 0) {
        std::cerr << "No se pudo abrir el puerto " << puerto << "\n";
        cerrarSocket(servidor);
        terminarSockets();
        return 1;
    }
    ::listen(servidor, 16);

    std::cout << "Interfaz web en http://localhost:" << puerto
              << "  (Ctrl+C para salir)\n";

    while (true) {
        const socket_t cliente = ::accept(servidor, nullptr, nullptr);
        // accept devuelve INVALID_SOCKET al fallar; en Windows SOCKET no tiene
        // signo, así que comparar contra cero no sirve.
        if (!socketValido(cliente)) continue;

        char buffer[4096] = {0};
        const auto leidos = ::recv(cliente, buffer, sizeof(buffer) - 1, 0);
        if (leidos <= 0) {
            cerrarSocket(cliente);
            continue;
        }
        std::istringstream peticion(buffer);
        std::string metodo, objetivo, version;
        peticion >> metodo >> objetivo >> version;

        const std::size_t interrogacion = objetivo.find('?');
        const std::string ruta = interrogacion == std::string::npos
                                     ? objetivo
                                     : objetivo.substr(0, interrogacion);
        const std::string consulta = interrogacion == std::string::npos
                                         ? ""
                                         : objetivo.substr(interrogacion + 1);
        const auto params = parametros(consulta);

        if (metodo != "GET") {
            responder(cliente, "405 Method Not Allowed", "text/plain; charset=utf-8",
                      "Solo GET");
        } else if (ruta == "/" || ruta == "/index.html") {
            std::string pagina = leerArchivo("web/index.html");
            if (pagina.empty()) pagina = leerArchivo("../web/index.html");
            if (pagina.empty()) {
                responder(cliente, "404 Not Found", "text/plain; charset=utf-8",
                          "No se encontró web/index.html");
            } else {
                responder(cliente, "200 OK", "text/html; charset=utf-8", pagina);
            }
        } else if (ruta == "/favicon.ico") {
            responder(cliente, "204 No Content", "image/x-icon", "");
        } else if (ruta.rfind("/api/", 0) == 0) {
            responder(cliente, "200 OK", "application/json; charset=utf-8",
                      manejarApi(estado, ruta, params));
        } else {
            responder(cliente, "404 Not Found", "text/plain; charset=utf-8", "No existe");
        }
        cerrarSocket(cliente);
    }

    cerrarSocket(servidor);
    terminarSockets();
    return 0;
}
