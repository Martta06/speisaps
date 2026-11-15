# GUÍA COMPLETA DEL BACKEND PARA LA IA DEL FRONTEND (`gale-map`)

> **Propósito de este documento:** Pásale este archivo `.md` directamente a la IA o persona que esté tocando el frontend (`gale-map/`). Contiene **absolutamente todo** cómo funciona el backend actual (`server.py` + motor en C `pathfinder/` + módulo ambiental `radiation_data/`), los JSON exactos de entrada/salida, las funciones en C y **los fallos concretos que tiene ahora mismo `gale-map/src/`** que están causando errores.

---

## 🚨 1. BUGS ACTUALES EN EL FRONTEND (`gale-map/src/`) QUE DEBES CORREGIR PRIMERO

Si el frontend está dando problemas al conectarse con el backend, revisa estos **4 puntos exactos** en `gale-map/src/`:

### Bug 1: `storms is not defined` en `gale-map/src/main.js` (líneas 102–125)
En `gale-map/src/main.js` tienes:
```js
addStormOverlay(map);
// const storms = addStormOverlay(map);  (Unirlo cuando esté el backend)
```
Pero más abajo en `refreshStorm()` (línea 125) hace `storms.updateStorm(data);`, lo que lanza un `ReferenceError: storms is not defined` cada 2 segundos cuando `/api/storm` responde `200 OK`.
- **Solución en `gale-map/src/main.js`:** Sustituye las líneas 102–104 por:
  ```js
  const storms = addStormOverlay(map);
  ```

### Bug 2: El Rover está en modo `demo: true` en `gale-map/src/main.js` (línea 152)
En `gale-map/src/main.js` línea 152 pone:
```js
addRover(map, { demo: true });
```
Con `demo: true`, `rover.js` **no llama nunca a `/api/rover`**.
- **Solución:** Cámbialo a `addRover(map, { demo: false });` para que consuma `/api/rover` del backend.

### Bug 3: `route.js` y el formato de coordenadas 2D vs 6D
En `gale-map/src/route.js` (línea 235), el frontend valida que cada punto sea `[lon, lat]`.
- Cuando llamas a `POST /api/route`, el backend devuelve un JSON con:
  - `data.coordinates`: Array 2D `[[lon, lat], [lon, lat], ...]` $\rightarrow$ **Listo para pintar la `LineString` en OpenLayers (`readRoutePoints(data)` lo lee automáticamente de `data.coordinates`)**.
  - `data.full_coordinates`: Array 6D `[[lon, lat, elev_m, elapsed_time_s, dose_rate_msv_day, temp_c], ...]` $\rightarrow$ **Úsalo para mostrar telemetría, perfiles de elevación, radiación y temperatura por tramo**.
  - `data.properties`: Objeto con todas las métricas globales de la ruta (`total_distance_m`, `total_duration_s`, `total_radiation_dose_msv`, `peak_dose_rate_msv_day`, `spe_active`, `min_temp_c`, `max_temp_c`, `max_slope_deg`, `cumulative_elevation_gain_m`).
- **¡OJO!** No pases `data.geojson` a `readRoutePoints()`, porque dentro de `data.geojson` las coordenadas son las 6D; pásale directamente `data` (como ya hace `readRoutePoints(data)`), o lee `data.coordinates`.

### Bug 4: El botón de Tormenta Solar (`solar.js`) no se está enviando en `POST /api/route`
En `gale-map/src/solar.js`, el botón `#solar-demo` cambia el estado visual de la alerta solar (`clear` $\rightarrow$ `forecast` $\rightarrow$ `active`), pero `route.js` no se entera al pedir ruta.
- **Solución:** En `route.js`, al hacer `fetch('/api/route', ...)`, lee si `#solar-alert` está activa (`!solarAlert.hidden` y `solarAlert.dataset.phase`) y envía en el body JSON:
  ```js
  speActive: !solarAlert.hidden,
  speRate: solarAlert.dataset.phase === 'active' ? 150.0 : (solarAlert.dataset.phase === 'forecast' ? 150.0 : 0.0),
  speOnset: solarAlert.dataset.phase === 'forecast' ? 600.0 : 0.0
  ```

---

## 🌐 2. ARQUITECTURA Y PUERTOS

- **Frontend (Vite + OpenLayers):** Corre en `http://localhost:5173` (`cd gale-map && npm run dev`).
- **Proxy configurado en `gale-map/vite.config.js`:** Cualquier petición desde el frontend a `/api/...` se redirige automáticamente a `http://127.0.0.1:8000/api/...`.
- **Backend HTTP (`server.py` en la raíz del repo):** Servidor Flask en `http://127.0.0.1:8000` (`python server.py`).
- **Motor C de Pathfinding (`pathfinder/martian_pathfinder` o `pathfinder/martian_pathfinder.exe`):** Ejecutable compilado en C11 con OpenMP que mapea en memoria (`mmap` / `MapViewOfFile`) el DEM de HiRISE (`Gale_DEM_SMG_1m.tif`, 5.4 GB), la LUT de blindaje topográfico (`radiation_data/gale_shielding_lut.bin`, $4010 \times 3600$) y la curva diurna de Curiosity RAD (`radiation_data/rad_baseline_24h.bin`).

---

## 🗺️ 3. SISTEMA DE COORDENADAS Y LÍMITES DEL MAPA

- **Proyección:** `ESRI:104905` / `IAU_2015:49900` (grados decimales en Marte, radio $R = 3\,396\,190\text{ m}$).
- **Orden siempre:** `[lon, lat]` (Longitud Este positiva, Latitud Norte positiva / Sur negativa).
- **Bounding Box exacto del DEM (`galeExtent`):**
  ```js
  const galeExtent = [
    137.122471132, // west  (min lon)
    -4.925474312,  // south (min lat)
    137.729812907, // east  (max lon)
    -4.248963057,  // north (max lat)
  ];
  ```
  *(Si envías a `/api/route` un punto fuera de este rectángulo, el backend devuelve `HTTP 400`).*

---

## 📡 4. CONTRATOS EXACTOS DE TODOS LOS ENDPOINTS REST (`/api/*`)

### 4.1. `POST /api/route` — Cálculo de Ruta Óptima ($A^*$ Espacio-Temporal + SPE)

#### Request JSON:
```json
{
  "origin": [137.4250, -4.5800],
  "destination": [137.4270, -4.5820],
  "localHour": 12.0,
  "mode": "eva",
  "stepMeters": 10,
  "speActive": false,
  "speRate": 0.0,
  "speMultiplier": 1.0,
  "speOnset": 0.0,
  "dustStorm": {
    "active": true,
    "center": [137.4255, -4.5805],
    "radiusM": 2500.0,
    "velEastMs": 10.0,
    "velNorthMs": 0.0,
    "severityPenalty": 25.0
  }
}
```
- `origin`, `destination` (**obligatorios**): `[lon, lat]` dentro de `galeExtent`.
- `localHour` (*opcional*, defecto `9.0`): Hora solar local marciana LMST `[0.0, 24.0)`.
- `mode` (*opcional*, defecto `"rover"`): `"eva"` (astronauta a pie, penaliza fuerte la radiación y pendientes $>20^\circ$) o `"rover"` (rover tipo Curiosity, blindaje $\times 0.85$, pendientes hasta $27^\circ$). También acepta `"isRover": 0 | 1`.
- `stepMeters` (*opcional*, defecto `10`): `10` (malla de 10 m, tarda `0.05s – 0.3s`) o `1` (malla de 1 m para rutas cortas).
- `speActive` (*opcional*, defecto `false`): Si es `true`, activa tormenta solar de partículas (`+150 uSv/h` por defecto si no se indica `speRate`).
- `speRate` (*opcional*, defecto `0.0`): Tasa extra de radiación SPE a cielo abierto en $\mu\text{Sv/h}$ (ej. `150.0` o `300.0`).
- `speMultiplier` (*opcional*, defecto `1.0`): Alternativa a `speRate`; multiplicador sobre el fondo GCR (ej. `5.0` o `10.0`).
- `speOnset` (*opcional*, defecto `0.0`): Segundos desde el inicio de la ruta en los que empieza la tormenta solar (`0.0` = tormenta ya activa desde la salida; `600.0` = la tormenta empieza a los 10 minutos de caminar).

#### Response JSON (`HTTP 200`):
```json
{
  "status": "ok",
  "calculation_time_s": 0.0542,
  "coordinates": [
    [137.425, -4.58],
    [137.4251687, -4.5801687]
  ],
  "full_coordinates": [
    [137.425, -4.58, -4501.25, 0.0, 0.6542, -8.45],
    [137.4251687, -4.5801687, -4501.10, 14.3, 0.6538, -8.42]
  ],
  "properties": {
    "total_distance_m": 2458.29,
    "total_duration_s": 2869.0,
    "total_cost": 5840.12,
    "max_slope_deg": 6.42,
    "cumulative_elevation_gain_m": 18.5,
    "total_radiation_dose_msv": 0.021338,
    "peak_dose_rate_msv_day": 0.6581,
    "spe_active": false,
    "min_temp_c": -12.4,
    "max_temp_c": -8.1,
    "waypoints_count": 195,
    "calculation_time_s": 0.0542,
    "is_rover": 0,
    "step_meters": 10
  },
  "geojson": {
    "type": "FeatureCollection",
    "crs": {
      "type": "name",
      "properties": { "name": "urn:ogc:def:crs:IAU_2015::49900" }
    },
    "features": [ ... ]
  }
}
```
> **Significado de las 6 columnas de `full_coordinates[i]`:**
> - `[0]`: `lon_deg` (grados Este)
> - `[1]`: `lat_deg` (grados Norte)
> - `[2]`: `elevation_m` (altitud en metros desde el datum marciano, ej. `-4500.0`)
> - `[3]`: `elapsed_time_s` (segundos acumulados de marcha desde el origen)
> - `[4]`: `dose_rate_msv_day` (tasa de dosis instantánea en ese punto en $\text{mSv/día}$; para pasar a $\mu\text{Sv/h}$ multiplica por `1000 / 24`)
> - `[5]`: `temp_c` (temperatura del aire en ese punto y hora en $^\circ\text{C}$)

---

### 4.2. `POST /api/camp` — Búsqueda de Refugio / Campamento ("Necesito acampar")

#### Request JSON:
```json
{
  "position": [137.4250, -4.5800],
  "localHour": 14.0
}
```

#### Response JSON (`HTTP 200`):
```json
{
  "status": "ok",
  "camp_found": true,
  "camp_location": {
    "coordinates": [137.433, -4.586],
    "name": "Campamento de Refugio Alpha (Planicie Segura)",
    "slope_deg": 2.1,
    "roughness": 0.08,
    "radiation_shielding": "Óptimo (depresión topográfica local)",
    "suitability_score": 94.5
  },
  "route_to_camp": {
    "coordinates": [[137.425, -4.58], [137.4252, -4.5801], ...],
    "distance_m": 619.65,
    "duration_s": 647.42,
    "radiation_dose_msv": 0.004924
  }
}
```

---

### 4.3. `GET /api/rover` — Posición y Telemetría del Rover en Tiempo Real

#### Response JSON (`HTTP 200`):
```json
{
  "position": [137.389025, -4.646509],
  "timestamp": "2026-11-15T10:17:27.220119+00:00",
  "speed_ms": 0.85,
  "heading_deg": 45.0,
  "mode": "autonomous_drive"
}
```

---

### 4.4. `GET /api/storm` — Radar de Tormenta de Polvo

#### Response JSON (`HTTP 200`):
```json
{
  "center": [137.348077, -4.650000],
  "diameterM": 8500.0,
  "dangerLevel": 3,
  "motion": "approaching",
  "windSpeedMs": 24.5,
  "simulated": true,
  "provenance": "Mars Environmental Dynamics Analyzer (MEDA) Simulator"
}
```

---

### 4.5. `GET /api/health` — Comprobación de Estado del Backend

#### Response JSON (`HTTP 200`):
```json
{
  "status": "ok",
  "service": "martian_pathfinder_api",
  "dem_loaded": "Gale_DEM_SMG_1m.tif",
  "dem_exists": true,
  "bin_exists": true
}
```

---

## ⚙️ 5. FUNCIONES EN C PARA CONSULTAR RADIACIÓN Y TEMPERATURA (`radiation_data/mars_env.h`)

Si algún módulo en C o puente nativo necesita consultar directamente la radiación ($\mu\text{Sv/h}$) o la temperatura ($^\circ\text{C}$) en coordenadas `(lon_deg, lat_deg)` sin pasarles punteros a la LUT ni al DEM (los cargan automáticamente en caché `static` en la primera llamada):

```c
#include "radiation_data/mars_env.h"

// 1. Consulta de 1 punto (O(1), ~20 ns tras la primera llamada)
float get_radiation_dose_rate_frontend(
    double lon_deg,
    double lat_deg,
    float t_lmst,
    float spe_multiplier // 1.0 = sin tormenta, >1.0 (ej. 5.0 o 10.0) = tormenta SPE activa
);

float get_mars_temperature_frontend(
    double lon_deg,
    double lat_deg,
    float t_lmst
);

// 2. Consulta por lotes (array de N coordenadas)
void get_radiation_dose_rate_batch_frontend(
    const double *lons_deg,
    const double *lats_deg,
    size_t count,
    float t_lmst,
    float spe_multiplier,
    float *out_rates
);

void get_mars_temperature_batch_frontend(
    const double *lons_deg,
    const double *lats_deg,
    size_t count,
    float t_lmst,
    float *out_temps_c
);

// 3. Rejilla 2D completa para el mapa de calor de environment.js (fila 0 = Sur, col 0 = Oeste)
void get_environment_grid_frontend(
    double west_lon,
    double south_lat,
    double east_lon,
    double north_lat,
    int grid_rows,
    int grid_cols,
    float t_lmst,
    float spe_multiplier,
    float *out_rad,
    float *out_temp_c
);
```

---

## 🖥️ 6. USO DIRECTO DEL BINARIO C POR CONSOLA (`martian_pathfinder`)

```bash
cd pathfinder

# Sintaxis:
./martian_pathfinder <start_lon> <start_lat> <goal_lon> <goal_lat> [hora_lmst] [is_rover: 0|1] [step_m: 10|1] [out_geojson] [dem_tif] [--spe <uSv/h>] [--spe-onset <s>] [--spe-end <s>] [--shield <f>]

# Ejemplo en calma:
./martian_pathfinder 137.4250 -4.5800 137.4450 -4.6000 9.0 0 10 route_calm.geojson

# Ejemplo con tormenta solar SPE activa de +150 uSv/h:
./martian_pathfinder 137.4250 -4.5800 137.4450 -4.6000 9.0 0 10 route_spe.geojson --spe 150
```
