/*
 * dem_reader.h — Lector de alto rendimiento para el DEM de HiRISE (BigTIFF) en C.
 *
 * Utiliza mmap(2) para acceso aleatorio O(1) directo a los 5.4 GB sin cargar
 * la imagen completa en memoria RAM física (paginación por demanda del kernel).
 */

#ifndef DEM_READER_H
#define DEM_READER_H

#include <stdbool.h>
#include <stddef.h>

/* Metadatos extraídos de la cabecera BigTIFF de Gale_DEM_SMG_1m.tif */
typedef struct {
    int fd;
    void *h_file;              /* HANDLE en Windows */
    void *h_map;               /* HANDLE FileMapping en Windows */
    void *map_base;            /* Puntero base devuelto por mmap / MapViewOfFile */
    size_t file_size;
    const float *raster_data;  /* Puntero mmap al array de elevaciones */
    size_t width;              /* 36000 */
    size_t height;             /* 40100 */
    int    downsample;         /* 1 para TIF 1m/px (5.4 GB), 10 para hirise_dem.bin 10m/px (55 MB) */
    double origin_lon;         /* 137.12247113204074 */
    double origin_lat;         /* -4.248963057145466 */
    double pixel_scale_lon;    /* 1.687060485811425e-05 */
    double pixel_scale_lat;    /* 1.687060485811425e-05 */
    float  nodata_val;         /* -3.402823e+38 */
} DemContext;

/* Abre el archivo TIF y mapea la memoria. Retorna true si tuvo éxito. */
bool dem_open(const char *filepath, DemContext *ctx);

/* Cierra el archivo y libera el mapeo de memoria. */
void dem_close(DemContext *ctx);

/* Comprueba si unas coordenadas (x, y) de píxel caen dentro del DEM */
static inline bool dem_in_bounds(const DemContext *ctx, int x, int y) {
    return (x >= 0 && (size_t)x < ctx->width && y >= 0 && (size_t)y < ctx->height);
}

/* Lee la elevación en metros en el píxel (x, y). Retorna true si es válida. */
bool dem_get_elevation(const DemContext *ctx, int x, int y, float *out_z);

/* Convierte coordenadas geográficas marcianas (lon, lat) a píxel (x, y) */
void dem_geo_to_pixel(const DemContext *ctx, double lon_deg, double lat_deg, int *out_x, int *out_y);

/* Convierte píxel (x, y) a coordenadas geográficas marcianas (lon, lat) */
void dem_pixel_to_geo(const DemContext *ctx, int x, int y, double *out_lon_deg, double *out_lat_deg);

/* Calcula la pendiente local en grados en el píxel (x, y) usando diferencias centrales */
bool dem_get_slope_deg(const DemContext *ctx, int x, int y, double *out_slope_deg);

/*
 * dem_get_roughness — Calcula la micro-rugosidad superficial normalizada [0.0, 1.0]
 * en el píxel (x, y) usando la desviación estándar de elevación en una ventana 3x3.
 *
 * sigma_max = 0.50 m (rugosidad de saturación en Gale Crater).
 */
bool dem_get_roughness(const DemContext *ctx, int x, int y, double *out_roughness);

#endif /* DEM_READER_H */
