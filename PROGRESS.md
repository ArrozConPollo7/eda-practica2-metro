# PROGRESS

Fase actual: 9 (interfaz web). Fases 1 a 9 completas y verificadas. Entrega lista.
Decisiones:
- ids de estación tipo slug (`san_antonio`); la columna `linea` lleva "A|K" en los intercambiadores.
- grafo expandido (estación, línea): el transbordo es una arista más, con penalización configurable (3,0 min por defecto).
- minutos = 60 · distancia_haversine / velocidad de la línea (A 36, B 31, K y J 14, L 18, H y M 16, T-A 17 km/h).
- A*: v_max = 1,05 · máxima velocidad observada; h precalculada una vez por nodo.
- la red es un árbol (46 estaciones, 45 tramos): Yen devuelve 1 ruta y se valida con un grafo sintético con ciclo.
- salidas del programa: out/route.{dot,json,svg} (o out/grafo.* si no hay ruta).

Comandos: `cmake -S . -B build && cmake --build build -j4` ; `./build/navegador` ; `./build/navegador --web` ; `./build/pruebas`.
Interfaz web: src/servidor.cpp (sockets POSIX, API JSON) + web/index.html (mapa SVG interactivo).
Reporte: `cd report && pdflatex reporte.tex` dos veces (o tectonic). Genera reporte.pdf de 23 páginas.
Entrega: practica2_eda.zip (25 archivos, sin build/) + report/reporte.pdf (23 páginas).
