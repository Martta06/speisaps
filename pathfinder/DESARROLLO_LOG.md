# LOG DE DESARROLLO — SUBSISTEMA PATHFINDER (PERSONA 2)
**Proyecto:** Speisaps / Martian Google Maps (Cráter Gale)  
**Módulo:** Terreno, Grafos, Función de Costes y Pathfinding  
**Lenguaje:** C11 (Optimizado, sin dependencias externas pesadas)

---

## [2026-11-14 16:55] — Fase 1: Arquitectura de la función de coste y geometría marciana
### Archivos creados:
1. **`params.h` / `params.c`**:
   - `TraversalMode`: Modos `MODE_FOOT_EVA` y `MODE_ROVER`.
   - `TraversalLimits`: Umbrales de bloqueo duro (hard constraints): pendiente máxima, altura máxima de roca, viento crítico, opacidad $\tau$, inercia térmica mínima segura (`ti_safe`) y alerta global de partículas solares (SEP).
   - `CostWeights`: Pesos configurables en tiempo de ejecución para cada factor ($w_{\text{dist}}, w_{\text{slope}}, w_{\text{rough}}, w_{\text{soft}}, w_{\text{rock}}, w_{\text{elev\_up}}, w_{\text{elev\_down}}, w_{\text{prox}}, w_{\text{unk}}, w_{\text{rad}}, w_{\text{wind}}$).
   - `EdgeParams`: Datos geométricos y ambientales por arista/tramo de navegación.
   - Perfiles predeterminados configurados según literatura científica de Curiosity (MSL) y biofísica de trajes presurizados EVA.

2. **`mars_geo.h` / `mars_geo.c`**:
   - Radio marciano medio de referencia: $R = 3\,396\,190\text{ m}$ (IAU 2015).
   - `mars_haversine_m`: Distancia geodésica exacta en la esfera marciana.
   - `mars_bearing_deg`: Rumbo de navegación (0° = Norte, horario).
   - `mars_slope_deg`: Pendiente angular en grados a partir de $\Delta z$ y $\Delta s$.
   - `mars_project_local` / `mars_unproject_local`: Proyección ENU ortogonal local ultrarrápida para áreas acotadas con error submilimétrico.
   - `mars_wind_crosstrack_factor`: Factor de viento lateral perpendicular que desestabiliza a un astronauta.

3. **`terrain_cost.h` / `terrain_cost.c`**:
   - Factores de coste individuales normalizados en $[0, 1]$ o retorno estricto de `INFINITY` ante bloqueo:
     - `slope_cost`: Función cuadrática $( \theta / \theta_{\max} )^2$.
     - `roughness_cost`: Variación superficial / rugosidad $VRM$.
     - `trafficability_cost`: Riesgo de atrapamiento en arena blanda usando inercia térmica $TI$.
     - `rock_abundance_cost`: Densidad de bloques de roca.
     - `hazard_proximity_cost`: Buffer de exclusión dura y decaimiento exponencial $e^{-d/d_0}$ en escarpes.
     - `elevation_cost`: Asimetría entre trabajo de ascenso vs. esfuerzo articular de descenso.
     - `unknown_cell_cost`: Penalización configurable para celdas inexploradas.
     - `radiation_cost_per_meter`: Dosis acumulada de radiación cósmica (GCR de Curiosity RAD: $0.64\text{ mSv/día}$).
     - `wind_cost`: Impacto de velocidad y componente lateral del viento.

4. **`edge_cost.h` / `edge_cost.c`**:
   - `edge_cost`: Combinación lineal y multiplicativa modular para optimización en grafos.
   - `edge_cost_breakdown`: Desglose individual de cada componente de coste para facilitar la generación de heatmaps y explicaciones de ruta en la interfaz gráfica.

5. **`test_cost.c` / `Makefile`**:
   - Batería de 42 pruebas unitarias automatizadas (100% PASS).

---

## [2026-11-14 17:25] — Fase 2: Inspección y lectura de datos GeoTIFF de HiRISE
### Archivo analizado: `Gale_DEM_SMG_1m.tif` (5.4 GB)
- **Formato:** BigTIFF (Little-Endian, Magic 43).
- **Dimensiones:** $36\,000 \times 40\,100$ píxeles.
- **Tipo de dato:** `Float32` (IEEE 754), 1 muestra por píxel, sin compresión.
- **Resolución:** $\Delta x = \Delta y = 1.68706 \times 10^{-5}$ grados $\approx \mathbf{1.000\text{ m/px}}$.
- **Origen:** Lon $137.12247^\circ\text{ E}$, Lat $-4.24896^\circ\text{ S}$.
- **Estructura física:**
  - $40\,100$ tiras (1 fila por tira).
  - Offset de inicio de datos ráster: byte `642249`.
  - Disposición 100% contigua en disco: fila $y$ comienza en $642249 + y \times 144\,000$.
  - Desplazamiento del píxel $(x, y)$: $\text{offset} = 642249 + (y \times 36\,000 + x) \times 4$.
- **Estrategia de optimización implementada en C:**
  - Mapeo en memoria virtual vía `mmap(2)` (`PROT_READ`, `MAP_SHARED`).
  - No consume 5.4 GB de RAM física: el kernel de Linux realiza paginación por demanda (*page fault*) solo para los píxeles de la ventana de navegación evaluados por A*.

---

## [2026-11-14 17:35] — Fase 3: Lector DEM BigTIFF, Algoritmo A* y Exportador GeoJSON
### Archivos implementados:
1. **`dem_reader.h` / `dem_reader.c`**:
   - Apertura del BigTIFF con `mmap(2)`.
   - Conversión bidireccional entre coordenadas geográficas marcianas $(\text{lon}, \text{lat})$ y píxeles del ráster $(x, y)$.
   - Lectura de elevación $z(x, y)$ con comprobación de `GDAL_NODATA`.
   - Cálculo del gradiente local y vector normal mediante diferencias centrales para obtener la pendiente en grados en cada nodo.
2. **`pathfinder_astar.h` / `pathfinder_astar.c`**:
   - Implementación del algoritmo A* con cola de prioridad / heap binario mínimo (`MinHeap`).
   - Conectividad de 8 vecinos con ponderación euclídea ($\Delta s = 1.0\text{ m}$ ortogonales, $\Delta s = \sqrt{2}\text{ m}$ diagonales).
   - Heurística admisible basada en distancia geodésica y cota inferior de coste por metro.
   - Evaluación en cada arista a través de `edge_cost()`.
3. **`geojson_export.h` / `geojson_export.c`**:
   - Generación de `FeatureCollection` GeoJSON con la geometría `LineString` de la ruta óptima.
   - Encabezado con CRS marciano oficial `urn:ogc:def:crs:IAU_2015::49900`.
   - Inclusión de metadatos de telemetría en las propiedades: distancia total recorrida, desnivel acumulado, pendiente máxima, coste total y número de waypoints para renderizado inmediato en el mapa OpenLayers del frontend.
4. **Verificación experimental (Ruta de 1 km)**:
   - Se ejecutó el algoritmo sobre un trayecto de $1\,023.64\text{ m}$ (750 waypoints).
   - Salida exportada a `route_1km.geojson`.
   - Renderizado con perfil altimétrico en `route_1km_rendered.png`.
