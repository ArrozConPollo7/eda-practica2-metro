#!/usr/bin/env python3
"""Construye los CSV de las rutas urbanas de Medellín (capa VM_Movilidad).

Fuente: https://www.medellin.gov.co/servidormapas/rest/services/transporte/VM_Movilidad/MapServer
- capa 2 "Rutas de Transporte Público": 504 polilíneas (código + recorrido).
- capa 0 "Parada": 10.043 filas (parada, ruta) con nro_parada, que ya trae el
  orden dentro del recorrido. No hay que proyectar nada.

Uso:
    python3 tools/construir_grafo_urbanos.py

Escribe:
    data/urb_paradas.csv   id,nombre,lat,lon,ruta
    data/urb_tramos.csv    origen,destino,minutos,ruta
    data/urb_acceso.csv    origen,destino,minutos

El id de cada parada es "u<objectid>": una parada física que sirve a tres rutas
aparece tres veces, que es justo lo que necesita el grafo expandido.
"""

import csv
import json
import math
import os
import urllib.request

SERVICIO = ("https://www.medellin.gov.co/servidormapas/rest/services/"
            "transporte/VM_Movilidad/MapServer")
CACHE = "/tmp/urb"
DATOS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")

VELOCIDAD_BUS_KMH = 18.0
ESPERA_POR_PARADA_MIN = 0.4
VELOCIDAD_PIE_KMH = 4.8
ESPERA_BUS_MIN = 5.0
RADIO_ACCESO_M = 400.0
UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/130.0 Safari/537.36")


def descargar(capa, nombre):
    ruta = os.path.join(CACHE, nombre + ".geojson")
    if os.path.exists(ruta):
        return json.load(open(ruta))["features"]
    os.makedirs(CACHE, exist_ok=True)
    features = []
    offset = 0
    while True:
        url = (f"{SERVICIO}/{capa}/query?where=1%3D1&outFields=*&outSR=4326&f=geojson"
               f"&resultOffset={offset}&resultRecordCount=1000")
        pedido = urllib.request.Request(url, headers={"User-Agent": UA})
        with urllib.request.urlopen(pedido, timeout=120) as r:
            tanda = json.load(r)["features"]
        features.extend(tanda)
        if len(tanda) < 1000:
            break
        offset += 1000
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
    paradas = descargar(0, "paradas")
    rutas = descargar(2, "rutas")
    estaciones = leer_estaciones()

    # Una fila por (parada, ruta). El nombre puede venir vacío o con commas.
    filas = []
    for f in paradas:
        p = f["properties"]
        lon, lat = f["geometry"]["coordinates"][:2]
        nombre = " ".join((p.get("direccion") or "").split()) or f"Parada {p['codigo_ruta']}"
        # El lector de CSV del programa no maneja comillas: se quitan aquí.
        nombre = nombre.replace('"', "").replace(",", " ")
        filas.append({
            "id": "u%d" % p["objectid"],
            "nombre": nombre[:60],
            "lat": lat,
            "lon": lon,
            "ruta": p["codigo_ruta"],
            "recorrido": p["recorrido"],
            "orden": p["nro_parada"],
        })

    # Tramos entre paradas consecutivas del mismo (código, recorrido).
    grupos = {}
    for fila in filas:
        grupos.setdefault((fila["ruta"], fila["recorrido"]), []).append(fila)

    tramos = []
    saltados = 0
    for (codigo, _), grupo in grupos.items():
        if len(grupo) < 2:
            saltados += 1
            continue
        grupo.sort(key=lambda x: (x["orden"], x["id"]))
        for a, b in zip(grupo, grupo[1:]):
            km = haversine_m(a["lat"], a["lon"], b["lat"], b["lon"]) / 1000.0
            if km <= 0.03 or km > 3.0:   # saltos por datos sucios
                continue
            minutos = round(km / VELOCIDAD_BUS_KMH * 60.0 + ESPERA_POR_PARADA_MIN, 2)
            tramos.append((a["id"], b["id"], minutos, codigo))
            tramos.append((b["id"], a["id"], minutos, codigo))

    # Acceso a pie parada <-> estación.
    acceso = []
    for fila in filas:
        for eid, _, lat, lon in estaciones:
            metros = haversine_m(fila["lat"], fila["lon"], lat, lon)
            if metros <= RADIO_ACCESO_M:
                caminando = round(metros / 1000.0 / VELOCIDAD_PIE_KMH * 60.0 + 0.5, 2)
                acceso.append((fila["id"], eid, caminando))
                acceso.append((eid, fila["id"], round(caminando + ESPERA_BUS_MIN, 2)))

    with open(os.path.join(DATOS, "urb_paradas.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["id", "nombre", "lat", "lon", "ruta"])
        for fila in filas:
            w.writerow([fila["id"], fila["nombre"], fila["lat"], fila["lon"], fila["ruta"]])
    with open(os.path.join(DATOS, "urb_tramos.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["origen", "destino", "minutos", "ruta"])
        w.writerows(tramos)
    with open(os.path.join(DATOS, "urb_acceso.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["origen", "destino", "minutos"])
        w.writerows(acceso)

    ids_metro = {e[0] for e in estaciones}
    tocadas = {x for a in acceso for x in (a[0], a[1]) if x in ids_metro}
    print("rutas con paradas:", len(grupos), "| grupos de 1 parada descartados:", saltados)
    print("paradas (fila, ruta):", len(filas))
    print("tramos:", len(tramos), "| accesos a pie:", len(acceso))
    print("estaciones con parada a <400 m:", len(tocadas), "de", len(estaciones))


if __name__ == "__main__":
    main()
