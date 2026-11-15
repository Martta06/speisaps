# Guía de Integración Ambiental (Radiación y Temperatura) para el Frontend

Este documento explica cómo utilizar las funciones auxiliares de radiación y temperatura marciana desde el motor / backend que alimenta la interfaz del mapa de **Gale Crater**.

---

## 1. Archivos necesarios para compilar

Incluye la cabecera en tu código C/C++:
```c
#include "radiation_data/mars_env.h"
```

Y añade al comando de compilación los archivos fuente:
- `radiation_data/mars_env.c`
- `pathfinder/mars_geo.c`

Ejemplo con `gcc`:
```bash
gcc -O3 tu_archivo.c radiation_data/mars_env.c pathfinder/mars_geo.c -lm -fopenmp -o servidor_env
```

> **Nota sobre los archivos de datos:** Las funciones buscan automáticamente `hirise_meta.txt`, `rad_baseline_24h.bin`, `gale_shielding_lut.bin` y `hirise_dem.bin` tanto en el directorio actual como en `radiation_data/` o `../radiation_data/`, por lo que no hace falta pasarles rutas ni punteros a memoria.

---

## 2. Firmas de las funciones

### Radiación (`uSv/h`)
```c
float get_radiation_dose_rate_frontend(
    double lon_deg,
    double lat_deg,
    float t_lmst,
    float spe_multiplier
);
```
- **`lon_deg`**: Longitud marciana en grados decimales Este (ej. `137.4250`). Rango del mapa de Gale: `[137.1225, 137.7298]`.
- **`lat_deg`**: Latitud marciana en grados decimales Norte (ej. `-4.5800`). Rango del mapa de Gale: `[-4.9255, -4.2490]`.
- **`t_lmst`**: Hora solar local marciana decimal en `[0.0, 24.0)` (ej. `14.5f` para las 14:30 LMST).
- **`spe_multiplier`**: Multiplicador de tormenta solar (SPE):
  - `1.0f`: Condiciones nominales (sin tormenta solar, solo radiación cósmica galáctica GCR modulada por la presión diurna y el relieve).
  - `> 1.0f` (ej. `5.0f` o `10.0f`): Tormenta solar SPE activa; añade el exceso de partículas solares ponderado por el factor de visión de cielo abierto (`SVF_eff`) de ese píxel, de modo que los cañones y paredes rocosas protegen significativamente más que las llanuras expuestas.
- **Retorno**: Tasa de dosis efectiva instantánea en **$\mu\text{Sv/h}$** (valores nominales típicos entre `~15 uSv/h` en zonas muy escarpadas y `~28 uSv/h` en llanura abierta).

---

### Temperatura (`°C`)
```c
float get_mars_temperature_frontend(
    double lon_deg,
    double lat_deg,
    float t_lmst
);
```
- **`lon_deg`**, **`lat_deg`**: Coordenadas geográficas marcianas en grados decimales.
- **`t_lmst`**: Hora solar local marciana `[0.0, 24.0)`.
- **Retorno**: Temperatura del aire cercano a la superficie en **grados Celsius (`°C`)**, calculada con el ciclo diurno asimétrico de Parton-Logan calibrado con datos de Curiosity REMS y ajustada por la altitud real del DEM en esa coordenada (`-2.0 K/km`). Valores típicos entre `-81 °C` (madrugada) y `-13 °C` (tarde).

---

### Conversión auxiliar `(lon, lat) -> (row, col)` (opcional)
Si en algún punto necesitas conocer el píxel exacto de la matriz:
```c
#include "pathfinder/mars_geo.h"

int row, col;
mars_lonlat_to_pixel(lon_deg, lat_deg, 10.0, &row, &col); // 10.0 m/px para LUT, 1.0 m/px para DEM 1m
```

---

### Consultas por lotes (Array de puntos o Rejilla 2D del mapa)

Aunque llamar a las funciones de 1 punto dentro de un bucle en C es rapidísimo (porque la LUT y el DEM se quedan guardados en variables `static` en RAM tras la primera llamada), también tienes disponibles versiones **por lotes (batch)** y **por rejilla 2D**:

```c
// 1. Para evaluar un vector de N puntos (ej. todos los waypoints de una ruta de golpe)
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

// 2. Para rellenar de golpe la malla 2D del mapa (ej. 36x32 en environment.js)
// Usa la misma convención que OpenLayers: fila 0 = borde Sur, col 0 = borde Oeste.
// Puedes pasar NULL a out_rad o a out_temp_c si solo necesitas una de las dos métricas.
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

## 3. Ejemplo de uso

```c
#include <stdio.h>
#include "radiation_data/mars_env.h"

int main(void) {
    double lon = 137.4250;
    double lat = -4.5800;
    float hora_lmst = 14.0f; // 14:00 LMST

    // 1. Consulta sin tormenta solar (spe_multiplier = 1.0)
    float rad_normal = get_radiation_dose_rate_frontend(lon, lat, hora_lmst, 1.0f);

    // 2. Consulta durante tormenta SPE severa (x10)
    float rad_tormenta = get_radiation_dose_rate_frontend(lon, lat, hora_lmst, 10.0f);

    // 3. Consulta de temperatura en °C
    float temp_c = get_mars_temperature_frontend(lon, lat, hora_lmst);

    printf("Lon: %.4f, Lat: %.4f @ %.1f LMST\n", lon, lat, hora_lmst);
    printf("  Radiación nominal:  %.2f uSv/h\n", rad_normal);
    printf("  Radiación con SPE:  %.2f uSv/h\n", rad_tormenta);
    printf("  Temperatura:        %.2f °C\n", temp_c);

    return 0;
}
```

---

## 4. Rendimiento y coste computacional

- **Primera llamada (lazy loading automático):**
  - La primera vez que se llama a `get_radiation_dose_rate_frontend` o `get_mars_temperature_frontend`, el módulo carga en memoria estática la malla de $4010 \times 3600$ (`~55 MB` para la LUT y `~55 MB` para el DEM).
  - Tiempo de carga inicial desde SSD: **~15 a 30 milisegundos** (ocurre una única vez en toda la ejecución del programa).
- **Llamadas siguientes (tiempo real $O(1)$):**
  - Complejidad: **$O(1)$ estricto** (una proyección trigonométrica simple + un acceso directo a array en RAM).
  - Tiempo por consulta individual: **~15 – 30 nanosegundos** (`0.00002 ms`).
  - **Para rellenar la malla completa del frontend** (por ejemplo, una rejilla de $36 \times 32 = 1\,152$ celdas al mover el slider de la hora): tarda **menos de `0.05 milisegundos` (`< 50 µs`)**, por lo que puede llamarse a 60 FPS en tiempo real sin ningún lag.
