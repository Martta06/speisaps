/*
 * geojson_export.h — Generador de GeoJSON conforme al estándar y CRS marciano.
 */

#ifndef GEOJSON_EXPORT_H
#define GEOJSON_EXPORT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    double lon_deg;
    double lat_deg;
    double elevation_m;
} PathPoint;

typedef struct {
    PathPoint *points;
    size_t count;
    double total_distance_m;
    double total_cost;
    double max_slope_deg;
    double cumulative_elevation_gain_m;
} PathResult;

/*
 * Exporta el camino a una cadena en formato GeoJSON FeatureCollection.
 * Retorna true si se exportó correctamente.
 */
bool export_path_to_geojson_file(const char *filename, const PathResult *path);

/*
 * Escribe el GeoJSON en un buffer de memoria en string.
 */
int export_path_to_geojson_string(const PathResult *path, char *buffer, size_t max_len);

#endif /* GEOJSON_EXPORT_H */
