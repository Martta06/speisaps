// mars_env.c
#include "mars_env.h"
#include "../pathfinder/mars_geo.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    int f0;
    int c0;
    float w00, w01, w10, w11;
    float inv_dist_m;
} RayStepOffset;

static inline int clamp_index(int val, int min_val, int max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

static inline int is_valid_elevation(float z) {
    return !isnan(z) && !isinf(z) && (fabsf(z) <= 1.0e5f);
}

// Safe chunked I/O for files > 4 GB on Windows (avoids 32-bit msvcrt fread/fwrite overflow)
size_t fread_large(void *buffer, size_t elem_size, size_t total_count, FILE *stream) {
    char *ptr = (char *)buffer;
    size_t remaining = total_count;
    const size_t chunk_elems = 64ULL * 1024ULL * 1024ULL; // 64M floats = 256 MB per chunk

    while (remaining > 0) {
        size_t to_read = (remaining < chunk_elems) ? remaining : chunk_elems;
        size_t read_now = fread(ptr, elem_size, to_read, stream);
        if (read_now == 0) break;
        remaining -= read_now;
        ptr += read_now * elem_size;
    }
    return total_count - remaining;
}

size_t fwrite_large(const void *buffer, size_t elem_size, size_t total_count, FILE *stream) {
    const char *ptr = (const char *)buffer;
    size_t remaining = total_count;
    const size_t chunk_elems = 64ULL * 1024ULL * 1024ULL; // 256 MB per chunk

    while (remaining > 0) {
        size_t to_write = (remaining < chunk_elems) ? remaining : chunk_elems;
        size_t written_now = fwrite(ptr, elem_size, to_write, stream);
        if (written_now == 0) break;
        remaining -= written_now;
        ptr += written_now * elem_size;
    }
    return total_count - remaining;
}

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
) {
    float min_dist_m = (pixel_size_m > 2.0f) ? pixel_size_m : 2.0f;
    float log_ratio = logf(max_distance_m / min_dist_m);

    RayStepOffset *ray_table = (RayStepOffset *)malloc(
        (size_t)n_rays * (size_t)n_radial_steps * sizeof(RayStepOffset)
    );
    if (!ray_table) {
        fprintf(stderr, "Error: Failed to allocate ray offset table.\n");
        return;
    }

    // 1. Precompute ray geometry and bilinear weights (critical!)
    for (int k = 0; k < n_rays; ++k) {
        float phi = (float)(2.0 * M_PI * k / n_rays);
        float sin_phi = sinf(phi);
        float cos_phi = cosf(phi);

        for (int s = 0; s < n_radial_steps; ++s) {
            float t = (n_radial_steps > 1) ? ((float)s / (float)(n_radial_steps - 1)) : 0.0f;
            float dist_m = min_dist_m * expf(t * log_ratio);
            float step_px = dist_m / pixel_size_m;

            float df = step_px * sin_phi;
            float dc = step_px * cos_phi;
            int f0 = (int)floorf(df);
            int c0 = (int)floorf(dc);
            float wf = df - (float)f0;
            float wc = dc - (float)c0;

            RayStepOffset *entry = &ray_table[k * n_radial_steps + s];
            entry->f0 = f0;
            entry->c0 = c0;
            entry->w00 = (1.0f - wf) * (1.0f - wc);
            entry->w01 = (1.0f - wf) * wc;
            entry->w10 = wf * (1.0f - wc);
            entry->w11 = wf * wc;
            entry->inv_dist_m = 1.0f / dist_m;
        }
    }

    size_t total_px = (size_t)rows * (size_t)cols;

    // 2. Initialize accumulator in shielding_lut to 0.0f BEFORE ray loop
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < total_px; ++i) {
        shielding_lut[i] = 0.0f;
    }

    // 3. Single ray-casting loop with thread-safe progress & ETA
    double t_start = omp_get_wtime();
    size_t total_work_rows = (size_t)n_rays * (size_t)rows;
    size_t completed_rows = 0;

    for (int k = 0; k < n_rays; ++k) {
        const RayStepOffset *ray_steps = &ray_table[k * n_radial_steps];

        #pragma omp parallel for schedule(dynamic, 64)
        for (int r = 0; r < rows; ++r) {
            size_t row_offset = (size_t)r * (size_t)cols;
            for (int c = 0; c < cols; ++c) {
                size_t idx = row_offset + (size_t)c;
                float z0 = dem[idx];

                if (!is_valid_elevation(z0)) {
                    continue;
                }

                float max_slope = 0.0f;

                for (int s = 0; s < n_radial_steps; ++s) {
                    int nr = r + ray_steps[s].f0;
                    int nc = c + ray_steps[s].c0;

                    if (nr < 0 || nr + 1 >= rows || nc < 0 || nc + 1 >= cols) {
                        break;
                    }

                    size_t n_idx = (size_t)nr * (size_t)cols + (size_t)nc;
                    float z00 = dem[n_idx];
                    float z01 = dem[n_idx + 1];
                    float z10 = dem[n_idx + (size_t)cols];
                    float z11 = dem[n_idx + (size_t)cols + 1];

                    if (!is_valid_elevation(z00) || !is_valid_elevation(z01) ||
                        !is_valid_elevation(z10) || !is_valid_elevation(z11)) {
                        continue;
                    }

                    float z_interp = ray_steps[s].w00 * z00
                                   + ray_steps[s].w01 * z01
                                   + ray_steps[s].w10 * z10
                                   + ray_steps[s].w11 * z11;

                    float slope = (z_interp - z0) * ray_steps[s].inv_dist_m;
                    if (slope > max_slope) {
                        max_slope = slope;
                    }
                }

                shielding_lut[idx] += 1.0f / (1.0f + max_slope * max_slope);
            }

            size_t done;
            #pragma omp atomic capture
            done = ++completed_rows;

            if ((done & 511) == 0 || done == total_work_rows) {
                double elapsed = omp_get_wtime() - t_start;
                double pct = (100.0 * (double)done) / (double)total_work_rows;
                double eta = (elapsed / (double)done) * (double)(total_work_rows - done);
                #pragma omp critical
                {
                    printf("  -> Progress: %6.2f%% | Ray %2d/%d | Elapsed: %5.1fs | ETA: %5.1fs   \r",
                           pct, k + 1, n_rays, elapsed, eta);
                    fflush(stdout);
                }
            }
        }
    }
    printf("\nNormalizing LUT...\n");

    // 4. Normalize by number of rays and apply regolith albedo floor (0.15)
    float inv_n_rays = 1.0f / (float)n_rays;
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < total_px; ++i) {
        if (!is_valid_elevation(dem[i])) {
            shielding_lut[i] = 1.0f;
        } else {
            float svf_eff = shielding_lut[i] * inv_n_rays;
            shielding_lut[i] = albedo_floor + (1.0f - albedo_floor) * svf_eff;
        }
    }

    free(ray_table);
}

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
) {
    int safe_row = clamp_index(row, 0, rows - 1);
    int safe_col = clamp_index(col, 0, cols - 1);

    float t = fmodf(t_lmst, 24.0f);
    if (t < 0.0f) t += 24.0f;

    int h0 = ((int)t) % 24;
    int h1 = (h0 + 1) % 24;
    float frac = t - (float)((int)t);

    float d_rad_open_sky = (1.0f - frac) * baseline_curve_24h[h0] + frac * baseline_curve_24h[h1];

    float topo_factor = shielding_lut[(size_t)safe_row * (size_t)cols + (size_t)safe_col];
    float svf_eff = (topo_factor - 0.15f) / 0.85f;
    if (svf_eff < 0.0f) svf_eff = 0.0f;

    float total_external_rate = (d_rad_open_sky * topo_factor) + (spe_storm_rate_uSv_h * svf_eff);
    return total_external_rate * rover_shielding;
}

float get_safe_elevation(
    const float *dem,
    int row,
    int col,
    int rows,
    int cols,
    float default_elevation_m
) {
    int safe_row = clamp_index(row, 0, rows - 1);
    int safe_col = clamp_index(col, 0, cols - 1);

    float z = dem[(size_t)safe_row * (size_t)cols + (size_t)safe_col];
    if (!is_valid_elevation(z)) {
        return default_elevation_m;
    }
    return z;
}

MarsTemperature get_mars_temperature(
    float t_lmst,
    float elevation_m,
    float t_min_k,
    float t_max_k,
    float base_elevation_m
) {
    float t = fmodf(t_lmst, 24.0f);
    if (t < 0.0f) t += 24.0f;

    const float t_sunrise = 6.0f;
    const float t_sunset = 18.0f;
    const float thermal_lag_h = 2.5f;
    const float nocturnal_decay = 2.2f;

    float day_length = t_sunset - t_sunrise;
    float denom = day_length + 2.0f * thermal_lag_h;
    float f_sunset = sinf((float)M_PI * day_length / denom);

    float normalized_cycle;
    if (t >= t_sunrise && t <= t_sunset) {
        normalized_cycle = sinf((float)M_PI * (t - t_sunrise) / denom);
    } else {
        float dt_night = fmodf(t - t_sunset + 24.0f, 24.0f);
        float night_length = 24.0f - day_length;
        float exp_b = expf(-nocturnal_decay);
        normalized_cycle = f_sunset
            * (expf(-nocturnal_decay * dt_night / night_length) - exp_b)
            / (1.0f - exp_b);
    }

    float lapse_correction_k = -0.002f * (elevation_m - base_elevation_m);

    MarsTemperature result;
    result.temp_k = t_min_k + (t_max_k - t_min_k) * normalized_cycle + lapse_correction_k;
    result.temp_c = result.temp_k - 273.15f;
    return result;
}

/* --- Caché estática para las funciones auxiliares del frontend --- */
static int g_meta_loaded = 0;
static int g_env_rows = 4010;
static int g_env_cols = 3600;
static float g_env_px_size_m = 10.0f;

static int g_rad_loaded = 0;
static float g_baseline_curve_24h[24];
static float *g_shielding_lut = NULL;

static int g_dem_loaded = 0;
static float *g_dem_grid = NULL;

static FILE *open_env_data_file(const char *filename, const char *mode) {
    const char *prefixes[] = {
        "",
        "radiation_data/",
        "../radiation_data/",
        "speisaps/radiation_data/"
    };
    char path[512];
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], filename);
        FILE *f = fopen(path, mode);
        if (f) return f;
    }
    return NULL;
}

static void ensure_env_meta_loaded(void) {
    if (g_meta_loaded) return;
    FILE *meta = open_env_data_file("hirise_meta.txt", "r");
    if (meta) {
        int r = 0, c = 0;
        float px = 10.0f;
        if (fscanf(meta, "%d %d %f", &r, &c, &px) == 3 && r > 0 && c > 0) {
            g_env_rows = r;
            g_env_cols = c;
            g_env_px_size_m = px;
        }
        fclose(meta);
    }
    g_meta_loaded = 1;
}

static void ensure_rad_data_loaded(void) {
    if (g_rad_loaded) return;
    ensure_env_meta_loaded();

    FILE *f_base = open_env_data_file("rad_baseline_24h.bin", "rb");
    if (!f_base || fread(g_baseline_curve_24h, sizeof(float), 24, f_base) != 24) {
        for (int h = 0; h < 24; ++h) {
            g_baseline_curve_24h[h] = 26.7f;
        }
    }
    if (f_base) fclose(f_base);

    size_t total_px = (size_t)g_env_rows * (size_t)g_env_cols;
    g_shielding_lut = (float *)malloc(total_px * sizeof(float));
    if (g_shielding_lut) {
        FILE *f_lut = open_env_data_file("gale_shielding_lut.bin", "rb");
        if (!f_lut || fread_large(g_shielding_lut, sizeof(float), total_px, f_lut) != total_px) {
            for (size_t i = 0; i < total_px; ++i) {
                g_shielding_lut[i] = 1.0f;
            }
        }
        if (f_lut) fclose(f_lut);
    }

    g_rad_loaded = 1;
}

static void ensure_dem_data_loaded(void) {
    if (g_dem_loaded) return;
    ensure_env_meta_loaded();

    size_t total_px = (size_t)g_env_rows * (size_t)g_env_cols;
    g_dem_grid = (float *)malloc(total_px * sizeof(float));
    if (g_dem_grid) {
        FILE *f_dem = open_env_data_file("hirise_dem.bin", "rb");
        if (!f_dem || fread_large(g_dem_grid, sizeof(float), total_px, f_dem) != total_px) {
            for (size_t i = 0; i < total_px; ++i) {
                g_dem_grid[i] = -4500.0f;
            }
        }
        if (f_dem) fclose(f_dem);
    }

    g_dem_loaded = 1;
}

float get_radiation_dose_rate_frontend(
    double lon_deg,
    double lat_deg,
    float t_lmst,
    float spe_multiplier
) {
    ensure_rad_data_loaded();

    int row = 0, col = 0;
    mars_lonlat_to_pixel(lon_deg, lat_deg, (double)g_env_px_size_m, &row, &col);

    float t = fmodf(t_lmst, 24.0f);
    if (t < 0.0f) t += 24.0f;
    int h0 = ((int)t) % 24;
    int h1 = (h0 + 1) % 24;
    float frac = t - (float)((int)t);
    float d_rad_open_sky = (1.0f - frac) * g_baseline_curve_24h[h0] + frac * g_baseline_curve_24h[h1];

    /* Si spe_multiplier > 1.0 (ej. 5.0 o 10.0), añadimos el exceso de radiación SPE */
    float mult = (spe_multiplier > 1.0f) ? spe_multiplier : 1.0f;
    float spe_storm_rate_uSv_h = (mult - 1.0f) * d_rad_open_sky;

    if (!g_shielding_lut) {
        return d_rad_open_sky * mult;
    }

    return get_radiation_dose_rate(
        t_lmst,
        row,
        col,
        g_env_rows,
        g_env_cols,
        g_baseline_curve_24h,
        g_shielding_lut,
        1.0f,
        spe_storm_rate_uSv_h
    );
}

float get_mars_temperature_frontend(
    double lon_deg,
    double lat_deg,
    float t_lmst
) {
    ensure_dem_data_loaded();

    int row = 0, col = 0;
    mars_lonlat_to_pixel(lon_deg, lat_deg, (double)g_env_px_size_m, &row, &col);

    float elevation_m = -4500.0f;
    if (g_dem_grid) {
        elevation_m = get_safe_elevation(
            g_dem_grid,
            row,
            col,
            g_env_rows,
            g_env_cols,
            -4500.0f
        );
    }

    MarsTemperature temp = get_mars_temperature(
        t_lmst,
        elevation_m,
        192.0f,
        260.0f,
        -4500.0f
    );
    return temp.temp_c;
}

void get_radiation_dose_rate_batch_frontend(
    const double *lons_deg,
    const double *lats_deg,
    size_t count,
    float t_lmst,
    float spe_multiplier,
    float *out_rates
) {
    if (!lons_deg || !lats_deg || !out_rates || count == 0) return;
    ensure_rad_data_loaded();

    for (size_t i = 0; i < count; ++i) {
        out_rates[i] = get_radiation_dose_rate_frontend(
            lons_deg[i], lats_deg[i], t_lmst, spe_multiplier
        );
    }
}

void get_mars_temperature_batch_frontend(
    const double *lons_deg,
    const double *lats_deg,
    size_t count,
    float t_lmst,
    float *out_temps_c
) {
    if (!lons_deg || !lats_deg || !out_temps_c || count == 0) return;
    ensure_dem_data_loaded();

    for (size_t i = 0; i < count; ++i) {
        out_temps_c[i] = get_mars_temperature_frontend(
            lons_deg[i], lats_deg[i], t_lmst
        );
    }
}

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
) {
    if (grid_rows <= 0 || grid_cols <= 0) return;
    if (out_rad) ensure_rad_data_loaded();
    if (out_temp_c) ensure_dem_data_loaded();

    double d_lon = (east_lon - west_lon) / (double)grid_cols;
    double d_lat = (north_lat - south_lat) / (double)grid_rows;

    for (int r = 0; r < grid_rows; ++r) {
        double lat = south_lat + ((double)r + 0.5) * d_lat;
        for (int c = 0; c < grid_cols; ++c) {
            double lon = west_lon + ((double)c + 0.5) * d_lon;
            size_t idx = (size_t)r * (size_t)grid_cols + (size_t)c;

            if (out_rad) {
                out_rad[idx] = get_radiation_dose_rate_frontend(
                    lon, lat, t_lmst, spe_multiplier
                );
            }
            if (out_temp_c) {
                out_temp_c[idx] = get_mars_temperature_frontend(
                    lon, lat, t_lmst
                );
            }
        }
    }
}

#ifdef BUILD_LUT_MAIN
int main(void) {
    FILE *meta = fopen("hirise_meta.txt", "r");
    if (!meta) {
        fprintf(stderr, "Run prepare_rad_inputs.py first to generate hirise_meta.txt\n");
        return 1;
    }
    int rows = 0, cols = 0;
    float px_size_m = 1.0f;
    if (fscanf(meta, "%d %d %f", &rows, &cols, &px_size_m) != 3) {
        fclose(meta);
        return 1;
    }
    fclose(meta);

    size_t total_px = (size_t)rows * (size_t)cols;
    printf("Allocating %.2f GB for DEM + LUT (%d x %d)...\n",
           (2.0 * total_px * sizeof(float)) / (1024.0 * 1024.0 * 1024.0), rows, cols);

    float *dem = (float *)malloc(total_px * sizeof(float));
    float *lut = (float *)malloc(total_px * sizeof(float));
    if (!dem || !lut) {
        fprintf(stderr, "Error: Not enough RAM to allocate buffers.\n");
        return 1;
    }

    FILE *f_dem = fopen("hirise_dem.bin", "rb");
    if (!f_dem || fread_large(dem, sizeof(float), total_px, f_dem) != total_px) {
        fprintf(stderr, "Error reading hirise_dem.bin\n");
        return 1;
    }
    fclose(f_dem);

    printf("Computing 2,500 m LUT in C (32 rays, 100 steps)...\n");
    compute_topographic_shielding_lut(dem, lut, rows, cols, px_size_m, 32, 2500.0f, 100, 0.15f);

    printf("Writing gale_shielding_lut.bin (%.2f GB)...\n",
           (total_px * sizeof(float)) / (1024.0 * 1024.0 * 1024.0));
    FILE *f_lut = fopen("gale_shielding_lut.bin", "wb");
    if (!f_lut || fwrite_large(lut, sizeof(float), total_px, f_lut) != total_px) {
        fprintf(stderr, "Error writing gale_shielding_lut.bin\n");
        return 1;
    }
    fclose(f_lut);
    printf("Saved gale_shielding_lut.bin successfully.\n");

    free(dem);
    free(lut);
    return 0;
}
#endif