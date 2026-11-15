/*
 * dem_reader.c — Implementación del lector mmap para BigTIFF Gale HiRISE.
 */

#include "dem_reader.h"
#include <stdio.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#define DEM_DATA_OFFSET_BYTES 642249ULL
#define DEM_BIN_10M_SIZE_BYTES (3600ULL * 4010ULL * sizeof(float))

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool dem_open(const char *filepath, DemContext *ctx) {
    if (!ctx || !filepath) return false;

    ctx->fd = -1;
    ctx->h_file = NULL;
    ctx->h_map = NULL;
    ctx->map_base = NULL;
    ctx->raster_data = NULL;
    ctx->downsample = 1;

#ifdef _WIN32
    HANDLE hFile = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(hFile, &sz)) {
        CloseHandle(hFile);
        return false;
    }
    ctx->file_size = (size_t)sz.QuadPart;
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) {
        CloseHandle(hFile);
        return false;
    }
    void *map = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!map) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }
    ctx->h_file = (void *)hFile;
    ctx->h_map = (void *)hMap;
    ctx->map_base = map;
#else
    ctx->fd = open(filepath, O_RDONLY);
    if (ctx->fd < 0) {
        return false;
    }

    struct stat st;
    if (fstat(ctx->fd, &st) < 0) {
        close(ctx->fd);
        ctx->fd = -1;
        return false;
    }
    ctx->file_size = (size_t)st.st_size;

    void *map = mmap(NULL, ctx->file_size, PROT_READ, MAP_SHARED, ctx->fd, 0);
    if (map == MAP_FAILED) {
        close(ctx->fd);
        ctx->fd = -1;
        return false;
    }
    ctx->map_base = map;
#endif

    if (ctx->file_size == DEM_BIN_10M_SIZE_BYTES) {
        /* Rejilla binaria pre-submuestreada a 10 m/px (3600 x 4010 floats, sin cabecera TIFF) */
        ctx->raster_data = (const float *)ctx->map_base;
        ctx->downsample = 10;
    } else if (ctx->file_size > DEM_DATA_OFFSET_BYTES) {
        /* GeoTIFF original 1 m/px (36000 x 40100 floats) */
        ctx->raster_data = (const float *)((const char *)ctx->map_base + DEM_DATA_OFFSET_BYTES);
        ctx->downsample = 1;
    } else {
        dem_close(ctx);
        return false;
    }

    /* Metadatos del sistema de coordenadas de Gale Crater */
    ctx->width  = 36000;
    ctx->height = 40100;
    ctx->origin_lon = 137.12247113204074;
    ctx->origin_lat = -4.248963057145466;
    ctx->pixel_scale_lon = 1.687060485811425e-05;
    ctx->pixel_scale_lat = 1.687060485811425e-05;
    ctx->nodata_val = -3.402823e+38f;

    return true;
}

void dem_close(DemContext *ctx) {
    if (!ctx) return;

#ifdef _WIN32
    if (ctx->map_base) {
        UnmapViewOfFile(ctx->map_base);
        ctx->map_base = NULL;
    }
    if (ctx->h_map) {
        CloseHandle((HANDLE)ctx->h_map);
        ctx->h_map = NULL;
    }
    if (ctx->h_file && ctx->h_file != INVALID_HANDLE_VALUE) {
        CloseHandle((HANDLE)ctx->h_file);
        ctx->h_file = NULL;
    }
#else
    if (ctx->map_base) {
        munmap(ctx->map_base, ctx->file_size);
        ctx->map_base = NULL;
    }
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
#endif
    ctx->raster_data = NULL;
}

bool dem_get_elevation(const DemContext *ctx, int x, int y, float *out_z) {
    if (!dem_in_bounds(ctx, x, y)) return false;

    int ds = (ctx->downsample > 1) ? ctx->downsample : 1;
    size_t raw_w = ctx->width / (size_t)ds;
    size_t bx = (size_t)(x / ds);
    size_t by = (size_t)(y / ds);
    size_t index = by * raw_w + bx;
    float z = ctx->raster_data[index];

    /* Comprobar nodata */
    if (z <= -1e30f || isnan(z)) {
        return false;
    }

    if (out_z) *out_z = z;
    return true;
}

void dem_geo_to_pixel(const DemContext *ctx, double lon_deg, double lat_deg, int *out_x, int *out_y) {
    double px = (lon_deg - ctx->origin_lon) / ctx->pixel_scale_lon;
    /* En imágenes raster el eje Y crece hacia el sur (latitud decreciente) */
    double py = (ctx->origin_lat - lat_deg) / ctx->pixel_scale_lat;

    if (out_x) *out_x = (int)round(px);
    if (out_y) *out_y = (int)round(py);
}

void dem_pixel_to_geo(const DemContext *ctx, int x, int y, double *out_lon_deg, double *out_lat_deg) {
    if (out_lon_deg) *out_lon_deg = ctx->origin_lon + (double)x * ctx->pixel_scale_lon;
    if (out_lat_deg) *out_lat_deg = ctx->origin_lat - (double)y * ctx->pixel_scale_lat;
}

bool dem_get_slope_deg(const DemContext *ctx, int x, int y, double *out_slope_deg) {
    int d = (ctx->downsample > 1) ? ctx->downsample : 1;
    if (!dem_in_bounds(ctx, x - d, y) || !dem_in_bounds(ctx, x + d, y) ||
        !dem_in_bounds(ctx, x, y - d) || !dem_in_bounds(ctx, x, y + d)) {
        return false;
    }

    float z_w, z_e, z_n, z_s;
    if (!dem_get_elevation(ctx, x - d, y, &z_w) ||
        !dem_get_elevation(ctx, x + d, y, &z_e) ||
        !dem_get_elevation(ctx, x, y - d, &z_n) ||
        !dem_get_elevation(ctx, x, y + d, &z_s)) {
        return false;
    }

    double dz_dx = (double)(z_e - z_w) / (2.0 * (double)d);
    double dz_dy = (double)(z_s - z_n) / (2.0 * (double)d);

    double grad = sqrt(dz_dx * dz_dx + dz_dy * dz_dy);
    double slope_rad = atan(grad);

    if (out_slope_deg) *out_slope_deg = slope_rad * (180.0 / M_PI);
    return true;
}

bool dem_get_roughness(const DemContext *ctx, int x, int y, double *out_roughness) {
    int d = (ctx->downsample > 1) ? ctx->downsample : 1;
    if (!dem_in_bounds(ctx, x - d, y - d) || !dem_in_bounds(ctx, x + d, y + d)) {
        if (out_roughness) *out_roughness = 0.0;
        return false;
    }

    float vals[9];
    int count = 0;
    double sum = 0.0;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            float z;
            if (dem_get_elevation(ctx, x + dx * d, y + dy * d, &z)) {
                vals[count++] = z;
                sum += (double)z;
            }
        }
    }

    if (count < 4) {
        if (out_roughness) *out_roughness = 0.0;
        return false;
    }

    double mean = sum / (double)count;
    double var_sum = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = (double)vals[i] - mean;
        var_sum += diff * diff;
    }
    double sigma_z = sqrt(var_sum / (double)count);

    /* Normalizar con sigma_max escalado por resolución */
    double norm_rough = sigma_z / (0.50 * (double)d);
    if (norm_rough > 1.0) norm_rough = 1.0;

    if (out_roughness) *out_roughness = norm_rough;
    return true;
}
