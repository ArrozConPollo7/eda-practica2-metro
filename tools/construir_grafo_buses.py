#!/usr/bin/env python3
"""Construye los CSV del grafo de buses alimentadoras a partir de Metro GIS.

Fuente: https://metrogis.metrodemedellin.gov.co (capa Hosted/ruta_alimentadora,
Hosted/Alimentadora_Parada). Uso:

    python3 tools/construir_grafo_buses.py

Descarga las capas (si no estan en /tmp/bus), ordena las paradas de cada ruta
proyectandolas sobre su polilinea y escribe:

    data/bus_paradas.csv   id,nombre,lat,lon,ruta
    data/bus_tramos.csv    origen,destino,minutos,ruta
    data/bus_acceso.csv    parada,estacion,minutos

No usa librerias externas: la proyeccion compara cada parada con los vertices
de la polilinea (los vertices estan a ~9 m, mucho menos que la separacion entre
paradas), asi que el orden sale correcto sin geometria exacta.
"""

import csv
import json
import math
import os
import urllib.request

BASE = "https://metrogis.metrodemedellin.gov.co/server2/rest/services/Hosted"
CACHE = "/tmp/bus"
DATOS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")

VELOCIDAD_BUS_KMH = 18.0   # comercial de una alimentadora
ESPERA_POR_PARADA_MIN = 0.4
VELOCIDAD_PIE_KMH = 4.8
ESPERA_BUS_MIN = 5.0
RADIO_ACCESO_M = 400.0
TOLERANCIA_RUTA_M = 90.0


def descargar(capa, capa_id, nombre):
    ruta = os.path.join(CACHE, nombre + ".geojson")
    if os.path.exists(ruta):
        return json.load(open(ruta))["features"]
    url = f"{BASE}/{capa}/FeatureServer/{capa_id}/query?where=1%3D1&outFields=*&outSR=4326&f=geojson"
    features = []
    offset = 0
    while True:
        with urllib.request.urlopen(f"{url}&resultOffset={offset}&resultRecordCount=1000", timeout=120) as r:
            tanda = json.load(r)["features"]
        features.extend(tanda)
        if len(tanda) < 1000:
            break
        offset += 1000
    os.makedirs(CACHE, exist_ok=True)
    json.dump({"type": "FeatureCollection", "features": features}, open(ruta, "w"))
    return features


def haversine_m(lat1, lon1, lat2, lon2):
    p = math.pi / 180.0
    a = (math.sin((lat2 - lat1) * p / 2) ** 2 +
         math.cos(lat1 * p) * math.cos(lat2 * p) * math.sin((lon2 - lon1) * p / 2) ** 2)
    return 2 * 6371000.0 * math.asin(math.sqrt(a))


def leer_estaciones():
    salida = []
    with open(os.path.join(DATOS, "stations.csv"), encoding="utf-8") as f:
        for fila in csv.DictReader(f):
            salida.append((fila["id"], fila["nombre"], float(fila["lat"]), float(fila["lon"])))
    return salida


def main():
    rutas = descargar("ruta_alimentadora", 2, "rutas")
    paradas = descargar("Alimentadora_Parada", 0, "paradas")
    estaciones = leer_estaciones()

    # Una fila por (codigo de ruta, polilinea). Cada polilinea es un sentido o
    # una variante del recorrido.
    lineas = {}
    for f in rutas:
        p = f["properties"]
        geom = f["geometry"]
        partes = [geom["coordinates"]] if geom["type"] == "LineString" else geom["coordinates"]
        # Las polilineas pueden venir partidas (MultiLineString): se unen en una
        # sola lista de vertices para proyectar las paradas sobre ella.
        coords = [(c[1], c[0]) for parte in partes for c in parte]  # lat, lon
        lineas.setdefault(p["ruta"], []).append(coords)

    # Paradas agrupadas por codigo de ruta.
    por_ruta = {}
    sin_nombre = 0
    for f in paradas:
        p = f["properties"]
        lon, lat = f["geometry"]["coordinates"][:2]
        # La fuente trae nombres con tabulaciones y 354 paradas sin nombre; se
        # normaliza el texto y se les pone un identificador legible.
        nombre = " ".join((p.get("parada") or p.get("label") or "").split())
        if not nombre:
            nombre = "Parada %s %d" % (p["ruta"], p["objectid"])
            sin_nombre += 1
        por_ruta.setdefault(p["ruta"], []).append({
            "id": "p%d" % p["objectid"],
            "nombre": nombre,
            "lat": lat,
            "lon": lon,
            "ruta": p["ruta"],
        })

    asignadas = {}   # id de parada -> (codigo, indice de linea, posicion)
    for codigo, grupos in por_ruta.items():
        polilineas = lineas.get(codigo, [])
        if not polilineas:
            continue
        for parada in grupos:
            mejor = None
            for i, coords in enumerate(polilineas):
                acumulado = 0.0
                for j, (lat, lon) in enumerate(coords):
                    if j > 0:
                        acumulado += haversine_m(coords[j - 1][0], coords[j - 1][1], lat, lon)
                    d = haversine_m(parada["lat"], parada["lon"], lat, lon)
                    if mejor is None or d < mejor[0]:
                        mejor = (d, i, acumulado, lat, lon)
            if mejor is not None and mejor[0] <= TOLERANCIA_RUTA_M:
                asignadas[parada["id"]] = (codigo, mejor[1], mejor[2])

    # Tramos: paradas consecutivas dentro de la misma polilinea.
    tramos = []
    for codigo, polilineas in lineas.items():
        for i in range(len(polilineas)):
            deltas = []
            for parada in por_ruta.get(codigo, []):
                dato = asignadas.get(parada["id"])
                if dato and dato[1] == i:
                    deltas.append((dato[2], parada))
            deltas.sort(key=lambda x: x[0])
            for (_, a), (_, b) in zip(deltas, deltas[1:]):
                km = haversine_m(a["lat"], a["lon"], b["lat"], b["lon"]) / 1000.0
                if km <= 0.05:
                    continue
                minutos = km / VELOCIDAD_BUS_KMH * 60.0 + ESPERA_POR_PARADA_MIN
                tramos.append((a["id"], b["id"], round(minutos, 2), codigo))
                tramos.append((b["id"], a["id"], round(minutos, 2), codigo))

    # Acceso a pie: parada <-> estacion del Metro. Subirse a un bus cuesta
    # ademas la espera media; bajarse no cuesta nada.
    acceso = []
    for parada in (p for grupo in por_ruta.values() for p in grupo):
        if parada["id"] not in asignadas:
            continue
        for eid, _, lat, lon in estaciones:
            metros = haversine_m(parada["lat"], parada["lon"], lat, lon)
            if metros <= RADIO_ACCESO_M:
                caminando = round(metros / 1000.0 / VELOCIDAD_PIE_KMH * 60.0 + 0.5, 2)
                acceso.append((parada["id"], eid, caminando))
                acceso.append((eid, parada["id"], round(caminando + ESPERA_BUS_MIN, 2)))

    os.makedirs(DATOS, exist_ok=True)
    with open(os.path.join(DATOS, "bus_paradas.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["id", "nombre", "lat", "lon", "ruta"])
        for grupo in por_ruta.values():
            for p in grupo:
                w.writerow([p["id"], p["nombre"], p["lat"], p["lon"], p["ruta"]])
    with open(os.path.join(DATOS, "bus_tramos.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["origen", "destino", "minutos", "ruta"])
        w.writerows(tramos)
    with open(os.path.join(DATOS, "bus_acceso.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["origen", "destino", "minutos"])
        w.writerows(acceso)

    print("rutas:", len(lineas), "| paradas:", sum(len(v) for v in por_ruta.values()),
          "| asignadas a una polilinea:", len(asignadas), "| sin nombre en la fuente:", sin_nombre)
    print("tramos de bus:", len(tramos), "| accesos a pie:", len(acceso))
    estaciones_tocadas = {a[1] for a in acceso if not a[1].startswith("p")}
    print("estaciones con al menos una parada a <400 m:", len(estaciones_tocadas), "de", len(estaciones))


if __name__ == "__main__":
    main()
