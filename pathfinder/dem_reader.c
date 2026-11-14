/*
 * dem_reader.c — Implementación del lector mmap para BigTIFF Gale HiRISE.
 */

#include "dem_reader.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <math.h>
#include <stdio.h>

#define DEM_DATA_OFFSET_BYTES 642249ULL

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool dem_open(const char *filepath, DemContext *ctx) {
    if (!ctx) return false;

    ctx->fd = open(filepath, O_RDONLY);
    if (ctx->fd < 0) {
        perror("dem_open: error abriendo archivo TIF");
        return false;
    }

    struct stat st;
    if (fstat(ctx->fd, &st) < 0) {
        perror("dem_open: fstat falló");
        close(ctx->fd);
        return false;
    }
    ctx->file_size = (size_t)st.st_size;

    /*
     * Mapeamos el archivo en memoria virtual compartida de solo lectura.
     * mmap mapea direcciones de página; pasamos offset 0 y sumamos DEM_DATA_OFFSET_BYTES.
     */
    void *map = mmap(NULL, ctx->file_size, PROT_READ, MAP_SHARED, ctx->fd, 0);
    if (map == MAP_FAILED) {
        perror("dem_open: mmap falló");
        close(ctx->fd);
        return false;
    }

    ctx->raster_data = (const float *)((const char *)map + DEM_DATA_OFFSET_BYTES);

    /* Metadatos extraídos de la cabecera */
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
    if (!ctx || ctx->fd < 0) return;
    if (ctx->raster_data) {
        void *map_start = (void *)((const char *)ctx->raster_data - DEM_DATA_OFFSET_BYTES);
        munmap(map_start, ctx->file_size);
        ctx->raster_data = NULL;
    }
    close(ctx->fd);
    ctx->fd = -1;
}

bool dem_get_elevation(const DemContext *ctx, int x, int y, float *out_z) {
    if (!dem_in_bounds(ctx, x, y)) return false;

    size_t index = (size_t)y * ctx->width + (size_t)x;
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
    if (!dem_in_bounds(ctx, x - 1, y) || !dem_in_bounds(ctx, x + 1, y) ||
        !dem_in_bounds(ctx, x, y - 1) || !dem_in_bounds(ctx, x, y + 1)) {
        return false;
    }

    float z_w, z_e, z_n, z_s;
    if (!dem_get_elevation(ctx, x - 1, y, &z_w) ||
        !dem_get_elevation(ctx, x + 1, y, &z_e) ||
        !dem_get_elevation(ctx, x, y - 1, &z_n) ||
        !dem_get_elevation(ctx, x, y + 1, &z_s)) {
        return false;
    }

    /* Cada píxel mide exactamente ~1.0 metro */
    double dz_dx = (double)(z_e - z_w) / 2.0;
    double dz_dy = (double)(z_s - z_n) / 2.0;

    double grad = sqrt(dz_dx * dz_dx + dz_dy * dz_dy);
    double slope_rad = atan(grad);

    if (out_slope_deg) *out_slope_deg = slope_rad * (180.0 / M_PI);
    return true;
}
