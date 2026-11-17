# PROGRESS

Entrega lista (17-nov-2026). Fases 1 a 4 cerradas y verificadas en Linux y en
Windows con MinGW-w64 (cross-compilado, 0 warnings en ambos).

## Modelo final

- Metro + 47 rutas alimentadoras: 46 estaciones, 1836 paradas, 1889 nodos
  (estación, línea) y 3794 aristas dirigidas.
- `--metro` deja el modelo original (53 nodos, 106 aristas) para comparar.
- Los CSV de bus los genera `tools/construir_grafo_buses.py` desde Metro GIS.

## Cambios de la revisión de entrega

- Se quitaron los tres `.bat` (compilar_windows, ejecutar_web, ejecutar_consola):
  Windows los marcaba con SmartScreen porque el zip trae Mark-of-the-Web, y el
  profesor recibe mejor un proyecto sin scripts ejecutables. README y reporte
  ahora documentan los dos comandos de CMake.
- `src/servidor.cpp`: bind a `127.0.0.1` en vez de `INADDR_ANY` (sin aviso del
  Firewall de Windows, la interfaz es local).
- `src/servidor.cpp`: `accept` se valida con `socketValido()` en vez de `< 0`;
  en Windows `SOCKET` no tiene signo y la comparación siempre era falsa
  (warning `-Wtype-limits` que solo aparece al compilar con MinGW).
- `.gitignore`: auxiliares de LaTeX.
- `CMakeLists.txt`: con MinGW se enlaza `-static`; sin eso el `.exe` importa
  `libstdc++-6.dll` y `libgcc_s_seh-1.dll` y muere al instante cuando el PATH del
  proceso no incluye `mingw64\bin` (caso de doble clic). Verificado en Windows con
  WinLibs GCC 16.2: el zip compila sin warnings, las 10 pruebas pasan y el `.exe`
  estático sirve la interfaz sin el PATH de MinGW.
- `Graph::indiceEstacion` ya no casa con texto vacío: 354 paradas sin nombre en la
  fuente hacían que `cerrada=` (vacío) resolviera a una parada y desactivara Yen.
- `escapar()` descarta caracteres de control (tabulaciones en nombres de parada
  rompían el JSON).
- Yen compara rutas por estaciones del Metro + buses usados, no por paradas: un
  rodeo peatonal ya no cuenta como alternativa distinta.
- `puntosDeArticulacion` filtra las paradas de bus (casi todas hojas): sin eso
  devolvía 1166 puntos en vez de 40.
- `practica2_eda.zip` regenerado: 31 archivos, sin `PROGRESS.md`, sin `.bat` ni
  `build/`, con `tools/` y los tres CSV de bus.

## Comandos

- Linux: `cmake -S . -B build && cmake --build build -j4`; `./build/pruebas`.
- Windows MinGW: `cmake -S . -B build -G "MinGW Makefiles" && cmake --build build`.
- Regenerar datos de bus: `python3 tools/construir_grafo_buses.py`.
- Reporte: `cd report && pdflatex reporte.tex` dos veces (27 páginas).
- Entrega: `practica2_eda.zip` + `report/reporte.pdf`.

## Verificación hecha

Zip descomprimido en carpeta limpia: compila sin warnings, las 10 pruebas pasan,
el servidor responde (`/api/ruta` con 3 alternativas y `/` con HTTP 200) y el PDF
del zip es el de 27 páginas. En Windows (MinGW GCC 16.2) se compiló y se verificó
el desvío por estación cerrada (42,5 min con Universidad cerrada).
