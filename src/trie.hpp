// trie.hpp - árbol de prefijos para el autocompletado de estaciones.
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "graph.hpp"  // normalizarTexto

// Trie: cada nodo representa un prefijo y guarda los valores (índices de
// estación) que terminan ahí. Buscar un prefijo cuesta O(largo del prefijo) y
// listar sus coincidencias O(largo + coincidencias). Los hijos se guardan en un
// arreglo fijo de 27 posiciones (a-z y espacio), así que cada paso es O(1).
class Trie {
public:
    // Inserta un texto asociado a un valor (se normaliza sin tildes).
    void insertar(const std::string& texto, int valor) {
        int actual = 0;
        for (char c : normalizarTexto(texto)) {
            const int p = posicion(c);
            if (p < 0) continue;  // caracteres fuera del alfabeto se ignoran
            if (nodos[actual].hijos[p] < 0) {
                nodos[actual].hijos[p] = static_cast<int>(nodos.size());
                nodos.push_back(Nodo());
            }
            actual = nodos[actual].hijos[p];
        }
        for (int existente : nodos[actual].valores) {
            if (existente == valor) return;  // ya estaba (nombre e id iguales)
        }
        nodos[actual].valores.push_back(valor);
    }

    // Valores de los textos que empiezan por el prefijo dado.
    std::vector<int> porPrefijo(const std::string& prefijo, int limite = 8) const {
        std::vector<int> salida;
        int actual = 0;
        for (char c : normalizarTexto(prefijo)) {
            const int p = posicion(c);
            if (p < 0) continue;
            actual = nodos[actual].hijos[p];
            if (actual < 0) return salida;
        }
        // Recorrido del subárbol con una pila explícita (DFS).
        std::vector<int> pila{actual};
        while (!pila.empty() && static_cast<int>(salida.size()) < limite) {
            const int v = pila.back();
            pila.pop_back();
            for (int valor : nodos[v].valores) {
                if (static_cast<int>(salida.size()) >= limite) break;
                // El mismo valor puede estar en dos ramas (nombre e id).
                if (std::find(salida.begin(), salida.end(), valor) != salida.end()) continue;
                salida.push_back(valor);
            }
            for (int i = 26; i >= 0; --i) {
                if (nodos[v].hijos[i] >= 0) pila.push_back(nodos[v].hijos[i]);
            }
        }
        return salida;
    }

private:
    struct Nodo {
        int hijos[27];
        std::vector<int> valores;
        Nodo() {
            for (int& h : hijos) h = -1;
        }
    };

    // Índice del carácter dentro de hijos[]: 0-25 letras, 26 el espacio.
    static int posicion(char c) {
        if (c >= 'a' && c <= 'z') return c - 'a';
        if (c == ' ') return 26;
        return -1;
    }

    std::vector<Nodo> nodos{Nodo()};  // nodos[0] es la raíz
};
