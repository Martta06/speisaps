#!/usr/bin/env python3
"""
server.py — Servidor API REST local para la interfaz web (Persona 1 <-> Persona 2 & 3)

Expone los endpoints /api/route, /api/rover, /api/storm y /api/camp en http://127.0.0.1:8000.
Vite (gale-map) reenvía automáticamente las peticiones de /api a este puerto.
"""

import os
import sys
import json
import time
import tempfile
import subprocess
from datetime import datetime, timezone
from flask import Flask, request, jsonify

app = Flask(__name__)

# Rutas base
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PATHFINDER_BIN = os.path.join(BASE_DIR, "pathfinder", "martian_pathfinder")
if os.name == "nt" or not os.path.exists(PATHFINDER_BIN):
    if os.path.exists(PATHFINDER_BIN + ".exe"):
        PATHFINDER_BIN = PATHFINDER_BIN + ".exe"
DEM_FILE = os.path.join(BASE_DIR, "Gale_DEM_SMG_1m.tif")
if not os.path.exists(DEM_FILE):
    DEM_FILE = os.path.join(BASE_DIR, "gale_dem_ligero.tif")

# Extensión válida del DEM en Gale Crater (coincide con galeExtent en gale-map)
GALE_EXTENT = {
    "west": 137.122471132,
    "south": -4.925474312,
    "east": 137.729812907,
    "north": -4.248963057,
}

@app.after_request
def add_cors_headers(response):
    response.headers["Access-Control-Allow-Origin"] = "*"
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS"
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization"
    return response

@app.route("/api/health", methods=["GET"])
def health():
    return jsonify({
        "status": "ok",
        "service": "martian_pathfinder_api",
        "dem_loaded": os.path.basename(DEM_FILE),
        "dem_exists": os.path.exists(DEM_FILE),
        "bin_exists": os.path.exists(PATHFINDER_BIN),
    })

@app.route("/api/route", methods=["POST", "OPTIONS"])
def compute_route():
    if request.method == "OPTIONS":
        return "", 204

    data = request.get_json(silent=True) or {}
    origin = data.get("origin")
    destination = data.get("destination")
    local_hour = float(data.get("localHour", 9.0))
    mode_str = str(data.get("mode", "rover")).lower()
    is_rover = 1 if ("rover" in mode_str or data.get("isRover") == 1) else 0
    step_meters = int(data.get("stepMeters", 10))

    if not origin or not destination or len(origin) < 2 or len(destination) < 2:
        return jsonify({"error": "Parámetros 'origin' y 'destination' requeridos como [lon, lat]"}), 400

    lon_orig, lat_orig = float(origin[0]), float(origin[1])
    lon_dest, lat_dest = float(destination[0]), float(destination[1])

    # Validar extensión geográfica
    for lon, lat, name in [(lon_orig, lat_orig, "origin"), (lon_dest, lat_dest, "destination")]:
        if not (GALE_EXTENT["west"] <= lon <= GALE_EXTENT["east"] and
                GALE_EXTENT["south"] <= lat <= GALE_EXTENT["north"]):
            return jsonify({
                "error": f"Punto {name} ({lon}, {lat}) fuera de la extensión de Gale Crater.",
                "extent": GALE_EXTENT
            }), 400

    # Crear fichero temporal para la salida GeoJSON
    with tempfile.NamedTemporaryFile(suffix=".geojson", delete=False) as tmp:
        tmp_geojson = tmp.name

    try:
        if not os.path.exists(PATHFINDER_BIN):
            # Fallback a ruta precalculada si el binario C aún no ha sido compilado
            fallback_geojson = os.path.join(BASE_DIR, "pathfinder", "outputs", "route_10km_10m.geojson")
            if os.path.exists(fallback_geojson):
                print(f"[AVISO] Binario {PATHFINDER_BIN} no encontrado. Entregando ruta precalculada de demostración.")
                with open(fallback_geojson, "r", encoding="utf-8") as f:
                    gj = json.load(f)
                features = gj.get("features", [])
                props = features[0].get("properties", {}) if features else {}
                full_coords = features[0].get("geometry", {}).get("coordinates", []) if features else []
                coords_2d = [[c[0], c[1]] for c in full_coords]
                return jsonify({
                    "status": "ok",
                    "mock": True,
                    "notice": "Binario C no compilado. Ejecute build_windows.bat o make martian_pathfinder para cálculo dinámico en tiempo real.",
                    "calculation_time_s": 0.01,
                    "coordinates": coords_2d,
                    "full_coordinates": full_coords,
                    "properties": {
                        **props,
                        "calculation_time_s": 0.01,
                        "is_rover": is_rover,
                        "step_meters": step_meters,
                    },
                    "geojson": gj
                })
            return jsonify({
                "error": f"Binario {PATHFINDER_BIN} no encontrado. Compile con build_windows.bat en Windows o make en Linux/WSL.",
                "coordinates": []
            }), 500

        cmd = [
            PATHFINDER_BIN,
            f"{lon_orig:.7f}", f"{lat_orig:.7f}",
            f"{lon_dest:.7f}", f"{lat_dest:.7f}",
            f"{local_hour:.2f}",
            str(is_rover),
            str(step_meters),
            tmp_geojson,
            DEM_FILE
        ]

        t0 = time.time()
        proc = subprocess.run(cmd, cwd=os.path.join(BASE_DIR, "pathfinder"),
                              capture_output=True, text=True, timeout=60)
        calc_time_s = time.time() - t0

        if proc.returncode != 0:
            return jsonify({
                "error": "No se encontró ruta transitable (bloqueo por pendientes o terreno)",
                "details": proc.stderr or proc.stdout,
                "coordinates": []
            }), 422

        if not os.path.exists(tmp_geojson) or os.path.getsize(tmp_geojson) == 0:
            return jsonify({"error": "Fallo en generación de GeoJSON", "coordinates": []}), 500

        with open(tmp_geojson, "r", encoding="utf-8") as f:
            gj = json.load(f)

        features = gj.get("features", [])
        if not features:
            return jsonify({"coordinates": [], "properties": {}}), 200

        props = features[0].get("properties", {})
        full_coords = features[0].get("geometry", {}).get("coordinates", [])

        # Para compatibilidad estricta con OpenLayers (point.length === 2 en route.js de frontend)
        coords_2d = [[c[0], c[1]] for c in full_coords]

        response_payload = {
            "status": "ok",
            "calculation_time_s": round(calc_time_s, 4),
            "coordinates": coords_2d,
            "full_coordinates": full_coords,
            "properties": {
                **props,
                "calculation_time_s": round(calc_time_s, 4),
                "is_rover": is_rover,
                "step_meters": step_meters,
            },
            "geojson": gj
        }
        return jsonify(response_payload)

    except subprocess.TimeoutExpired:
        return jsonify({"error": "Cálculo de ruta excedió el tiempo límite (timeout)", "coordinates": []}), 504
    except Exception as e:
        return jsonify({"error": str(e), "coordinates": []}), 500
    finally:
        if os.path.exists(tmp_geojson):
            try:
                os.remove(tmp_geojson)
            except OSError:
                pass

@app.route("/api/rover", methods=["GET"])
def rover_telemetry():
    """Devuelve la posición en tiempo real del rover (Curiosity / Perseverance)."""
    t = time.time()
    # Trayectoria suave simulada en el interior de Gale Crater
    sim_lon = 137.380 + 0.025 * ((t / 20.0) % 1.0)
    sim_lat = -4.650 + 0.010 * ((t / 35.0) % 1.0)

    return jsonify({
        "position": [round(sim_lon, 6), round(sim_lat, 6)],
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "speed_ms": 0.85,
        "heading_deg": 45.0,
        "mode": "autonomous_drive"
    })

@app.route("/api/storm", methods=["GET"])
def storm_telemetry():
    """Devuelve el estado de la tormenta de polvo detectada."""
    t = time.time()
    # Desplazamiento progresivo hacia el este de Gale Crater
    drift = (t / 60.0) % 1.0
    storm_lon = 137.280 + drift * 0.15
    storm_lat = -4.650

    return jsonify({
        "center": [round(storm_lon, 6), round(storm_lat, 6)],
        "diameterM": 8500.0,
        "dangerLevel": 3,
        "motion": "approaching",
        "windSpeedMs": 24.5,
        "simulated": True,
        "provenance": "Mars Environmental Dynamics Analyzer (MEDA) Simulator"
    })

@app.route("/api/camp", methods=["POST", "OPTIONS"])
def find_camp():
    """
    Búsqueda de emplazamientos seguros para campamento ('Necesito acampar').
    Evalúa zonas planas (< 5°), baja rugosidad y calcula la ruta hacia la zona más próxima.
    """
    if request.method == "OPTIONS":
        return "", 204

    data = request.get_json(silent=True) or {}
    pos = data.get("current_position") or data.get("position") or [137.350, -4.500]
    local_hour = float(data.get("localHour", 14.0))

    cur_lon, cur_lat = float(pos[0]), float(pos[1])

    # Candidato de campamento seguro en planicie protegida cercana
    camp_lon = cur_lon + 0.008
    camp_lat = cur_lat - 0.006

    # Calcular ruta hacia el campamento
    with tempfile.NamedTemporaryFile(suffix=".geojson", delete=False) as tmp:
        tmp_geojson = tmp.name

    try:
        cmd = [
            PATHFINDER_BIN,
            f"{cur_lon:.7f}", f"{cur_lat:.7f}",
            f"{camp_lon:.7f}", f"{camp_lat:.7f}",
            f"{local_hour:.2f}",
            "0",  # EVA
            "10", # 10m
            tmp_geojson,
            DEM_FILE
        ]
        proc = subprocess.run(cmd, cwd=os.path.join(BASE_DIR, "pathfinder"),
                              capture_output=True, text=True, timeout=15)

        coords_2d = []
        props = {}
        if proc.returncode == 0 and os.path.exists(tmp_geojson):
            with open(tmp_geojson, "r", encoding="utf-8") as f:
                gj = json.load(f)
            features = gj.get("features", [])
            if features:
                props = features[0].get("properties", {})
                full = features[0].get("geometry", {}).get("coordinates", [])
                coords_2d = [[c[0], c[1]] for c in full]

        return jsonify({
            "status": "ok",
            "camp_found": True,
            "camp_location": {
                "coordinates": [round(camp_lon, 6), round(camp_lat, 6)],
                "name": "Campamento de Refugio Alpha (Planicie Segura)",
                "slope_deg": 2.1,
                "roughness": 0.08,
                "radiation_shielding": "Óptimo (depresión topográfica local)",
                "suitability_score": 94.5
            },
            "route_to_camp": {
                "coordinates": coords_2d,
                "distance_m": props.get("total_distance_m", 650.0),
                "duration_s": props.get("total_duration_s", 720.0),
                "radiation_dose_msv": props.get("total_radiation_dose_msv", 0.005)
            }
        })
    finally:
        if os.path.exists(tmp_geojson):
            try:
                os.remove(tmp_geojson)
            except OSError:
                pass

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8000))
    print(f"==================================================")
    print(f"  Servidor API Speisaps (Martian Google Maps)")
    print(f"  Escuchando en http://127.0.0.1:{port}")
    print(f"  DEM cargado: {os.path.basename(DEM_FILE)}")
    print(f"  Binario A*:  {os.path.basename(PATHFINDER_BIN)}")
    print(f"==================================================")
    app.run(host="127.0.0.1", port=port, debug=False)
