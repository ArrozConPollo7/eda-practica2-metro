# PROGRESS

Entrega lista (17-nov-2026). Verificado en Linux y en Windows con MinGW-w64
(cross-compilado, 0 warnings en ambos).

## Modelo final

- Metro + 47 rutas alimentadoras + 209 rutas urbanas: 46 estaciones, 11 879
  paradas, 11 932 nodos (estación, línea) y 30 610 aristas dirigidas.
- `--metro` deja el modelo original (53 nodos, 106 aristas) para comparar.
- Datos: `tools/construir_grafo_buses.py` (Metro GIS) y
  `tools/construir_grafo_urbanos.py` (servidor de mapas de la Alcaldía,
  servicio VM_Movilidad, que ya trae `nro_parada`).

## Resultados que justifican los buses

- San Antonio→Arví pasa de 1 a 3 rutas; Poblado→Arví de 47,0 a 47,0/51,0/51,0.
- Cerrar Universidad: SA→Acevedo 27,3 min (antes, sin ruta).
- Cerrar San Antonio: Floresta→Poblado 22,8 min por Cisneros + bus
  «Anillo F2 Carrera 65» + línea A (antes, sin ruta).
- Puntos de articulación: siguen 40, pero cambian de dueño (salen San Antonio,
  Industriales y Poblado; entran La Aurora, Villa Sierra y Trece de Noviembre).
- A*: el ahorro de nodos va del 29 % (Poblado→Arví) al 99 % en trayectos cortos.

## Cambios de la revisión de entrega

- Sin `.bat`: Windows los marcaba con SmartScreen por el Mark-of-the-Web.
- `bind` a `127.0.0.1` (sin aviso del Firewall) y `accept` con `socketValido()`.
- `-static` con MinGW: sin eso el `.exe` no abre si falta la carpeta bin en el PATH.
- `indiceEstacion` no casa con texto vacío (354 paradas sin nombre desactivaban Yen).
- Yen compara por estaciones del Metro + buses usados, no por paradas.
- `puntosDeArticulacion`: set de pares en vez de matriz n×n (con 12 000 nodos
  pedía 140 MB) e incluye los accesos a pie.
- Carga con índices (`unordered_map`): 48 ms para 30 610 aristas. Sin ellos,
  `Graph::indiceEstacion`/`Graph::nodo` eran O(n) por fila.
- Autocompletado: las estaciones del Metro van primero (el Trie tiene 11 925
  entradas).
- `practica2_eda.zip`: 35 archivos, sin `PROGRESS.md`, sin `.bat`, sin `build/`.

## Comandos

- Linux: `cmake -S . -B build && cmake --build build -j4`; `./build/pruebas`.
- Windows MinGW: `cmake -S . -B build -G "MinGW Makefiles" && cmake --build build`.
- Datos: `python3 tools/construir_grafo_buses.py` y `..._urbanos.py`.
- Reporte: `cd report && pdflatex reporte.tex` dos veces (27 páginas).
- Entrega: `practica2_eda.zip` + `report/reporte.pdf`.

## Verificación hecha

Zip descomprimido en carpeta limpia: compila sin warnings, las 10 pruebas pasan,
el servidor responde (`/api/ruta` con 3 alternativas, el desvío de Floresta y
`/` con HTTP 200) y el PDF del zip es el de 27 páginas. En Windows (MinGW GCC
16.2) se compiló y se comprobó el mismo desvío.
