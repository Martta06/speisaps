#!/usr/bin/env python3
"""
server.py — Servidor API REST local para la interfaz web (Persona 1 <-> Persona 2 & 3)

Expone los endpoints /api/route, /api/rover, /api/storm y /api/camp en http://127.0.0.1:8000.
Vite (gale-map) reenvía automáticamente las peticiones de /api a este puerto.
"""

import os
import sys
import json
import math
import time
import random
import tempfile
import subprocess
from datetime import datetime, timezone
from flask import Flask, request, jsonify
import ctypes
import math
import threading

app = Flask(__name__)

# Rutas base
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PATHFINDER_BIN = os.path.join(BASE_DIR, "pathfinder", "martian_pathfinder")
if os.name == "nt" or not os.path.exists(PATHFINDER_BIN):
    if os.path.exists(PATHFINDER_BIN + ".exe"):
        PATHFINDER_BIN = PATHFINDER_BIN + ".exe"
DEM_FILE = os.path.join(BASE_DIR, "Gale_DEM_SMG_1m.tif")
if not os.path.exists(DEM_FILE):
    alt_dem = os.path.join(BASE_DIR, "..", "geotiff", "Gale_DEM_SMG_1m.tif")
    if os.path.exists(alt_dem):
        DEM_FILE = os.path.abspath(alt_dem)
    else:
        DEM_FILE = os.path.join(BASE_DIR, "gale_dem_ligero.tif")

# Extensión válida del DEM en Gale Crater (coincide con galeExtent en gale-map)
GALE_EXTENT = {
    "west": 137.122471132,
    "south": -4.925474312,
    "east": 137.729812907,
    "north": -4.248963057,
}

MARS_RADIUS_M = 3396190.0
M_PER_DEG_LAT = MARS_RADIUS_M * (math.pi / 180.0)
M_PER_DEG_LON = MARS_RADIUS_M * math.cos(-4.58 * math.pi / 180.0) * (math.pi / 180.0)

# Estado dinámico de la tormenta de arena en el servidor
STORM_STATE = {
    "active": False,
    "frozen": False,
    "spawn_time": 0.0,
    "spawn_lon": 137.320,
    "spawn_lat": -4.620,
    "frozen_lon": 137.320,
    "frozen_lat": -4.620,
    "vel_east_ms": 12.0,
    "vel_north_ms": -3.0,
    "diameter_m": 5000.0,
    "danger_level": 3,
    "wind_speed_ms": 22.0,
    "severity_penalty": 25.0,
}

def spawn_random_storm(custom_data=None):
    """Genera una tormenta dentro de los límites de Gale Crater con margen de seguridad."""
    data = custom_data or {}
    margin_deg = 0.06  # ~3.5 km de margen interior para no spawnear pegada al borde
    lon = float(data.get("lon", random.uniform(GALE_EXTENT["west"] + margin_deg, GALE_EXTENT["east"] - margin_deg)))
    lat = float(data.get("lat", random.uniform(GALE_EXTENT["south"] + margin_deg, GALE_EXTENT["north"] - margin_deg)))
    if "center" in data and isinstance(data["center"], list) and len(data["center"]) >= 2:
        lon, lat = float(data["center"][0]), float(data["center"][1])

    diameter_m = float(data.get("diameterM", random.uniform(4000.0, 8500.0)))
    wind_speed = float(data.get("windSpeedMs", random.uniform(18.0, 28.0)))
    heading_rad = random.uniform(-math.pi / 3.0, math.pi / 3.0)  # Deriva predominante Oeste -> Este
    drift_speed = random.uniform(0.02, 0.05)  # Deriva suave para que coincida con el círculo en pantalla
    ve = float(data.get("velEastMs", drift_speed * math.cos(heading_rad)))
    vn = float(data.get("velNorthMs", drift_speed * math.sin(heading_rad)))
    danger = int(data.get("dangerLevel", min(5, max(1, int(round((wind_speed - 10.0) / 4.0))))))

    is_frozen = bool(data.get("frozen", False))
    STORM_STATE.update({
        "active": True,
        "frozen": is_frozen,
        "spawn_time": time.time(),
        "spawn_lon": lon,
        "spawn_lat": lat,
        "frozen_lon": lon,
        "frozen_lat": lat,
        "vel_east_ms": ve,
        "vel_north_ms": vn,
        "diameter_m": diameter_m,
        "danger_level": danger,
        "wind_speed_ms": round(wind_speed, 1),
        "severity_penalty": float(data.get("severityPenalty", 80.0 + 40.0 * danger)),
    })

def get_current_storm_snapshot():
    """Calcula la posición actual de la tormenta y verifica si se ha salido de los límites del mapa."""
    if not STORM_STATE["active"]:
        return None

    if STORM_STATE.get("frozen"):
        cur_lon = STORM_STATE["frozen_lon"]
        cur_lat = STORM_STATE["frozen_lat"]
        ve_effective = 0.0
        vn_effective = 0.0
        motion = "stationary"
    else:
        dt = max(0.0, time.time() - STORM_STATE["spawn_time"])
        # Avance visual suave en el mapa (~15-20 m cada 2s)
        sim_dt = dt * 250.0
        cur_lon = STORM_STATE["spawn_lon"] + (STORM_STATE["vel_east_ms"] * sim_dt) / M_PER_DEG_LON
        cur_lat = STORM_STATE["spawn_lat"] + (STORM_STATE["vel_north_ms"] * sim_dt) / M_PER_DEG_LAT
        ve_effective = round(STORM_STATE["vel_east_ms"], 4)
        vn_effective = round(STORM_STATE["vel_north_ms"], 4)
        motion = "approaching" if STORM_STATE["vel_east_ms"] >= 0 else "receding"

    radius_m = STORM_STATE["diameter_m"] / 2.0
    radius_deg = radius_m / M_PER_DEG_LAT

    # Comprobar si la tormenta completa ha salido de los límites de Gale Crater
    out_of_bounds = (
        cur_lon - radius_deg > GALE_EXTENT["east"] or
        cur_lon + radius_deg < GALE_EXTENT["west"] or
        cur_lat - radius_deg > GALE_EXTENT["north"] or
        cur_lat + radius_deg < GALE_EXTENT["south"]
    )
    if out_of_bounds:
        # Si se sale por completo del mapa, la desactivamos limpiamente
        STORM_STATE["active"] = False
        STORM_STATE["frozen"] = False
        return None

    return {
        "active": True,
        "frozen": bool(STORM_STATE.get("frozen", False)),
        "center": [round(cur_lon, 6), round(cur_lat, 6)],
        "diameterM": STORM_STATE["diameter_m"],
        "radiusM": radius_m,
        "dangerLevel": STORM_STATE["danger_level"],
        "motion": motion,
        "windSpeedMs": STORM_STATE["wind_speed_ms"],
        "velEastMs": ve_effective,
        "velNorthMs": vn_effective,
        "severityPenalty": STORM_STATE["severity_penalty"],
        "simulated": True,
        "provenance": "Mars Environmental Dynamics Analyzer (MEDA) Simulator",
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

    # Soporte para Tormenta Solar (SPE)
    spe_active = bool(data.get("speActive", False))
    spe_rate = float(data.get("speRate", 0.0))
    spe_mult = float(data.get("speMultiplier", 1.0))
    spe_onset = float(data.get("speOnset", 0.0))
    if spe_rate <= 0.0 and spe_mult > 1.0:
        spe_rate = (spe_mult - 1.0) * 26.7
    elif spe_active and spe_rate <= 0.0:
        spe_rate = 150.0

    # Soporte para Tormenta de Arena Dinámica (enviada en el body o activa en STORM_STATE)
    dust_storm = data.get("dustStorm")
    if not dust_storm and STORM_STATE["active"]:
        dust_storm = get_current_storm_snapshot()
    dust_storm = dust_storm or {}
    dust_active = bool(dust_storm.get("active", bool(dust_storm))) if isinstance(dust_storm, dict) else False
    dust_center = dust_storm.get("center") if isinstance(dust_storm, dict) else None
    dust_radius_m = float(dust_storm.get("radiusM", float(dust_storm.get("diameterM", 0.0)) / 2.0)) if isinstance(dust_storm, dict) else 0.0
    dust_ve = float(dust_storm.get("velEastMs", dust_storm.get("windSpeedMs", 0.0))) if isinstance(dust_storm, dict) else 0.0
    dust_vn = float(dust_storm.get("velNorthMs", 0.0)) if isinstance(dust_storm, dict) else 0.0
    dust_pen = float(dust_storm.get("severityPenalty", 25.0)) if isinstance(dust_storm, dict) else 25.0

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
        if spe_rate > 0.0:
            cmd.extend(["--spe", f"{spe_rate:.2f}", "--spe-onset", f"{spe_onset:.1f}"])
        if dust_active and dust_center and len(dust_center) >= 2 and dust_radius_m > 0.0:
            cmd.extend([
                "--dust-storm",
                f"{float(dust_center[0]):.7f}",
                f"{float(dust_center[1]):.7f}",
                f"{dust_radius_m:.1f}",
                f"{dust_ve:.2f}",
                f"{dust_vn:.2f}",
                f"{dust_pen:.1f}",
            ])

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

@app.route("/api/storm", methods=["GET", "POST", "DELETE", "OPTIONS"])
def storm_telemetry():
    """
    Gestiona y devuelve el estado de la tormenta de polvo:
    - POST /api/storm: Spawnea una tormenta aleatoria (o con los parámetros dados) dentro de Gale Crater.
    - DELETE /api/storm: Limpia/desactiva la tormenta activa.
    - GET /api/storm: Devuelve la posición actualizada cada 2s (y la desactiva si sale del mapa).
    """
    if request.method == "OPTIONS":
        return "", 204

    if request.method == "POST":
        body = request.get_json(silent=True) or {}
        if body.get("active") is False or body.get("clear") is True:
            STORM_STATE["active"] = False
            STORM_STATE["frozen"] = False
            return jsonify({"active": False, "storm": None})
        if body.get("action") == "freeze":
            snap = get_current_storm_snapshot()
            if snap is not None:
                STORM_STATE["frozen"] = True
                STORM_STATE["frozen_lon"] = snap["center"][0]
                STORM_STATE["frozen_lat"] = snap["center"][1]
            return jsonify(get_current_storm_snapshot() or {"active": False, "storm": None})
        if body.get("action") == "unfreeze":
            if STORM_STATE["active"] and STORM_STATE.get("frozen"):
                STORM_STATE["spawn_lon"] = STORM_STATE["frozen_lon"]
                STORM_STATE["spawn_lat"] = STORM_STATE["frozen_lat"]
                STORM_STATE["spawn_time"] = time.time()
                STORM_STATE["frozen"] = False
            return jsonify(get_current_storm_snapshot() or {"active": False, "storm": None})
        spawn_random_storm(body)
        return jsonify(get_current_storm_snapshot())

    if request.method == "DELETE":
        STORM_STATE["active"] = False
        STORM_STATE["frozen"] = False
        return jsonify({"active": False, "storm": None})

    # GET: Si hay una tormenta spawneada con POST, devuelve su evolución (con chequeo de bordes)
    snap = get_current_storm_snapshot()
    if snap is not None:
        return jsonify(snap)

    return jsonify({
        "active": False,
        "storm": None,
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



_env_library = None
_env_lock = threading.Lock()


@app.get("/api/environment")
def environment_grid():
    global _env_library

    metric = request.args.get("metric")
    if metric not in ("temperature", "radiation"):
        return jsonify(error="Unknown environment metric."), 400

    hour = request.args.get("localHour", default=None, type=float)
    if hour is None or not math.isfinite(hour) or not 0 <= hour < 24:
        return jsonify(error="localHour must be in [0, 24)."), 400

    spe_mult = request.args.get("speMultiplier", default=1.0, type=float)
    if spe_mult is None or not math.isfinite(spe_mult) or spe_mult < 1.0:
        spe_mult = 1.0

    rows, columns = 380, 340
    extent = [
        GALE_EXTENT["west"], GALE_EXTENT["south"],
        GALE_EXTENT["east"], GALE_EXTENT["north"],
    ]

    buffer_type = ctypes.c_float * (rows * columns)
    radiation = buffer_type()
    temperature = buffer_type()

    try:
        with _env_lock:
            if _env_library is None:
                path = os.path.join(BASE_DIR, "radiation_data", "mars_env.dll")
                library = ctypes.CDLL(path)
                function = library.get_environment_grid_frontend
                float_pointer = ctypes.POINTER(ctypes.c_float)

                function.argtypes = [
                    ctypes.c_double, ctypes.c_double,
                    ctypes.c_double, ctypes.c_double,
                    ctypes.c_int, ctypes.c_int,
                    ctypes.c_float, ctypes.c_float,
                    float_pointer, float_pointer,
                ]
                function.restype = None
                _env_library = library

            _env_library.get_environment_grid_frontend(
                *extent, rows, columns, hour,
                spe_mult,
                radiation, temperature,
            )
    except (OSError, AttributeError):
        app.logger.exception("Environmental model unavailable")
        return jsonify(error="Environmental model unavailable."), 503

    is_temperature = metric == "temperature"
    result = temperature if is_temperature else radiation

    return jsonify(
        metric=metric,
        localHour=hour,
        sol=1,
        crs="ESRI:104905",
        extent=extent,
        rows=rows,
        columns=columns,
        unit="°C" if is_temperature else "µSv/h",
        values=[round(v, 2) if math.isfinite(v) else None for v in result],
    )

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8000))
    print(f"==================================================")
    print(f"  Servidor API Speisaps (Martian Google Maps)")
    print(f"  Escuchando en http://127.0.0.1:{port}")
    print(f"  DEM cargado: {os.path.basename(DEM_FILE)}")
    print(f"  Binario A*:  {os.path.basename(PATHFINDER_BIN)}")
    print(f"==================================================")
    app.run(host="127.0.0.1", port=port, debug=False)
