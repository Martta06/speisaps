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

#ifdef __cplusplus
}
#endif

#endif // MARS_ENV_H