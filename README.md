# Navegador de rutas: Metro de Medellín

Práctica 2 de Estructuras de Datos y Algoritmos (EAFIT). Navegador de rutas del
Metro de Medellín en C++17 usando solo la biblioteca estándar (sin librerías
externas). Carga el grafo desde CSV, calcula la ruta de menor tiempo con Dijkstra
y A*, genera `route.svg` y exporta el grafo a DOT y JSON.

Líneas incluidas: A, B, metrocables K, J, L, H, M y tranvía Ayacucho (T-A).
46 estaciones, 53 nodos (estación, línea).

## Compilar y ejecutar

Funciona igual en Windows y en Linux. Hace falta CMake y un compilador de C++17
(Visual Studio con la carga de trabajo "Desarrollo para el escritorio con C++",
o MinGW-w64). El servidor web usa Winsock en Windows y sockets POSIX en Linux.

### Windows

Desde la carpeta del proyecto, en "Símbolo del sistema" o PowerShell, con
MinGW-w64 (es el caso más común):

```bat
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

El ejecutable queda en `build\navegador.exe`. Hace falta que `g++` y
`mingw32-make` estén en el `PATH`; si CMake no encuentra el compilador, lo dice
en el primer paso.

> Si `cmake` responde `CMAKE_MAKE_PROGRAM is not set`, MinGW no está en el
> `PATH`. Windows no trae compilador de C++: hay que instalar MinGW-w64
> ([winlibs](https://winlibs.com/) o MSYS2) y añadir su carpeta `bin` al `PATH`
> antes de repetir los dos comandos.

Con Visual Studio (carga de trabajo "Desarrollo para el escritorio con C++") se
omite el generador y se compila la configuración Release:

```bat
cmake -S . -B build
cmake --build build --config Release
```

En ese caso el ejecutable queda en `build\Release\navegador.exe`.

Con MinGW, el `CMakeLists.txt` enlaza el ejecutable de forma estática: no
depende de `libstdc++-6.dll` ni de `libgcc_s_seh-1.dll`, así que corre desde
cualquier consola o con doble clic. (Sin ese enlace, el `.exe` se cierra al
instante si la carpeta `bin` de MinGW no está en el `PATH` del proceso.)

**Opción A, interfaz web** (lanzar desde la carpeta del proyecto, porque el
servidor busca `web\index.html` relativo a la carpeta actual):

```bat
build\navegador.exe --web
```

y abrir <http://localhost:8080>. El servidor escucha solo en `127.0.0.1`, así
que no expone el puerto a la red ni pide permisos de Firewall.

**Opción B, solo consola** (el `main` con el menú):

```bat
build\navegador.exe
```

Pruebas: `build\pruebas.exe` (o `build\Release\pruebas.exe`).

### Linux

```bash
cmake -S . -B build
cmake --build build -j4

./build/navegador              # menú de consola
./build/navegador --web        # interfaz web en http://localhost:8080
./build/navegador --web 9000 data   # otro puerto y otra carpeta de datos
./build/pruebas                # pruebas con assert
```

Desde `build/`, si no se pasa carpeta, el programa busca `data` y luego `../data`.

## Menú

1. Autocompletar estación (Trie, ignora tildes y mayúsculas)
2. Ruta más rápida (Dijkstra), imprime la ruta con líneas y transbordos
3. Comparación A* contra Dijkstra (nodos explorados y tiempo) en un par y en 20 pares aleatorios
4. Rutas alternativas (Yen), k entre 2 y 6
5. Simulación de estación cerrada
6. Puntos de articulación de la red (DFS con low-link)
7. Exportar a `out/route.{dot,json,svg}` (o `out/grafo.*` si no hay ruta)
8. Cambiar la penalización de transbordo (minutos)

## Interfaz web

`./build/navegador --web` levanta un servidor HTTP (sockets POSIX, sin
dependencias) que sirve `web/index.html` y una API JSON: `/api/grafo`,
`/api/estaciones?q=`, `/api/ruta?o=&d=&k=&transbordo=&cerrada=` y
`/api/articulacion`. La página dibuja el mapa en SVG, permite acercar y
arrastrar, fija origen y destino con dos clics y tiene el deslizador de
transbordo, el selector de rutas de Yen, el simulado de estación cerrada y la
comparación A* contra Dijkstra. El cálculo siempre queda en C++: el navegador
solo pide JSON y dibuja. La casilla **Mostrar paradas de bus** dibuja las 11 879
paradas como puntos grises (son muchos: se dibujan en ~75 ms, pero se leen mejor
con el mapa acercado).

## Buses

Además del Metro, el grafo incluye dos conjuntos de rutas de bus: las **47
alimentadoras** del Metro y las **209 rutas urbanas** de la Alcaldía, con
**11 879 paradas** en total. El grafo pasa de 53 a **11 932 nodos** y de 106 a
**30 610 aristas**, y las rutas pueden combinar metro y bus.

- Alimentadoras: portal del Metro (`metrogis.metrodemedellin.gov.co`, capas
  `ruta_alimentadora` y `Alimentadora_Parada`, REST sin token). La capa de
  paradas no trae la secuencia, así que el script la obtiene proyectando cada
  parada sobre la polilínea de su ruta.
- Urbanas: servidor de mapas de la Alcaldía
  (`medellin.gov.co/servidormapas`, servicio `VM_Movilidad`, capas 0 y 2). Aquí
  la parada sí trae `nro_parada`, el orden dentro del recorrido.

Cada parada entra como un nodo (parada, ruta), igual que las estaciones se
modelan como (estación, línea); el acceso a pie de una parada a una estación a
menos de 400 m se cobra como un transbordo, y subirse al bus suma 5 minutos de
espera. Los algoritmos no cambiaron.

Los CSV se regeneran con:

```bash
python3 tools/construir_grafo_buses.py     # data/bus_*.csv
python3 tools/construir_grafo_urbanos.py   # data/urb_*.csv
```

Para volver al modelo original, sin buses:

```
./build/navegador --metro        # 53 nodos, 106 aristas
```

## Datos

`data/stations.csv`: `id,nombre,linea,lat,lon`. En `linea` van las líneas de la
estación separadas por `|` (`A|B|T-A` en San Antonio).

`data/edges.csv`: `origen,destino,minutos`, con los ids de las estaciones. La
línea de cada tramo se deduce de la línea que comparten las dos estaciones.

Los nombres y las coordenadas vienen de los artículos de Wikipedia del Metro de
Medellín y de cada estación (coordenadas en grados decimales). Los **tiempos son
estimados**: no son datos oficiales, se calculan con
`t = 60 · distancia_haversine / v`, con una velocidad comercial por tipo de línea
(A 36, B 31, metrocables K y J 14, L 18, H y M 16, tranvía 17 km/h). Sumando los
tramos, la línea A da 40,2 min (el dato oficial de recorrido completo es 42 min).

## Estructura

```
src/graph.hpp/.cpp        estaciones, líneas, nodos (estación, línea), haversine
src/algorithms.hpp/.cpp   Dijkstra, A*, Yen, puntos de articulación
src/trie.hpp              autocompletado por prefijo
src/io.hpp/.cpp           CSV, DOT, JSON y SVG escritos a mano
src/servidor.hpp/.cpp     servidor HTTP y API JSON para la interfaz web
web/index.html            interfaz web (mapa interactivo, panel y comparación)
src/main.cpp              menú de consola, modo --web y --metro
tests/test_main.cpp       pruebas con assert
tools/construir_grafo_buses.py    descarga las capas del Metro y genera data/bus_*.csv
tools/construir_grafo_urbanos.py  descarga las capas de la Alcaldía y genera data/urb_*.csv
```

La red de las ocho líneas del Metro es un árbol (46 estaciones y 45 tramos), así
que entre dos estaciones existe un único camino simple: con `--metro`, Yen
devuelve una sola ruta en los datos reales. Con los buses cargados hay ciclos y Yen
encuentra alternativas donde pasa una ruta; en las nueve estaciones sin parada
cerca sigue devolviendo una sola, porque el camino es único de verdad.
