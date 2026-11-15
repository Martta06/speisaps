// mars_env.h
#ifndef MARS_ENV_H
#define MARS_ENV_H

#include <stddef.h>
#include <stdio.h>
size_t fread_large(void *buffer, size_t elem_size, size_t total_count, FILE *stream);
size_t fwrite_large(const void *buffer, size_t elem_size, size_t total_count, FILE *stream);

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temp_k;
    float temp_c;
} MarsTemperature;

/**
 * Precomputes the static topographic radiation shielding Look-Up Table (LUT)
 * up to max_distance_m (2,500 m) using isotropic ray-casting, logarithmic
 * radial steps, bilinear interpolation, and boundary clamping.
 */
void compute_topographic_shielding_lut(
    const float *dem,
    float *shielding_lut,
    int rows,
    int cols,
    float pixel_size_m,
    int n_rays,
    float max_distance_m,
    int n_radial_steps,
    float albedo_floor
);

/**
 * O(1) runtime lookup of instantaneous effective radiation dose rate (uSv/h)
 * at pixel (row, col) and Martian local time t_lmst [0, 24).
 */
float get_radiation_dose_rate(
    float t_lmst,
    int row,
    int col,
    int rows,
    int cols,
    const float baseline_curve_24h[24],
    const float *shielding_lut,
    float rover_shielding,
    float spe_storm_rate_uSv_h
);

/**
 * Safely retrieves elevation (in meters) from the HiRISE DEM array, clamping
 * out-of-bounds coordinates and falling back to default_elevation_m on NaN/NoData.
 */
float get_safe_elevation(
    const float *dem,
    int row,
    int col,
    int rows,
    int cols,
    float default_elevation_m
);

/**
 * Computes near-surface Martian air temperature in Gale Crater using a
 * Parton-Logan diurnal model bounded by MSL/REMS extremes (Martinez et al., 2017)
 * and adjusted by local elevation lapse rate (-2.0 K/km).
 */
MarsTemperature get_mars_temperature(
    float t_lmst,
    float elevation_m,
    float t_min_k,
    float t_max_k,
    float base_elevation_m
);

/**
 * Devuelve la tasa de radiación actual en uSv/h para una coordenada (lon, lat) y hora dadas.
 * Consulta automáticamente la LUT topográfica y la curva de 24h por debajo.
 * spe_multiplier: 1.0 por defecto (sin tormenta), o mayor si hay SPE activa (ej. 5.0 o 10.0).
 */
float get_radiation_dose_rate_frontend(
    double lon_deg,
    double lat_deg,
    float t_lmst,
    float spe_multiplier
);

/**
 * Devuelve la temperatura en grados Celsius para una coordenada (lon, lat) y hora dadas.
 * Consulta automáticamente la elevación del DEM por debajo.
 */
float get_mars_temperature_frontend(
    double lon_deg,
    double lat_deg,
    float t_lmst
);

/**
 * Evalúa un array de N coordenadas (lons_deg[i], lats_deg[i]) de una sola vez
 * y escribe las tasas de radiación (uSv/h) en out_rates[i].
 */
void get_radiation_dose_rate_batch_frontend(
    const double *lons_deg,
    const double *lats_deg,
    size_t count,
    float t_lmst,
    float spe_multiplier,
    float *out_rates
);

/**
 * Evalúa un array de N coordenadas (lons_deg[i], lats_deg[i]) de una sola vez
 * y escribe las temperaturas (°C) en out_temps_c[i].
 */
void get_mars_temperature_batch_frontend(
    const double *lons_deg,
    const double *lats_deg,
    size_t count,
    float t_lmst,
    float *out_temps_c
);

/**
 * Genera directamente una rejilla 2D de (grid_rows x grid_cols) para el mapa del frontend
 * en el rectángulo [west_lon, south_lat, east_lon, north_lat] (fila 0 = borde Sur, col 0 = Oeste,
 * igual que environment.js en OpenLayers).
 * Rellena out_rad (si != NULL) y/o out_temp_c (si != NULL) con tamaño grid_rows * grid_cols.
 */
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

#ifdef __cplusplus
}
#endif

#endif // MARS_ENV_H