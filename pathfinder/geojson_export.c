/*
 * geojson_export.c — Implementación de exportación GeoJSON con CRS IAU_2015::49900.
 */

#include "geojson_export.h"
#include <stdio.h>
#include <stdlib.h>

int export_path_to_geojson_string(const PathResult *path, char *buffer, size_t max_len) {
    if (!path || !buffer || max_len == 0) return -1;

    size_t offset = 0;
    int written = snprintf(buffer + offset, max_len - offset,
        "{\n"
        "  \"type\": \"FeatureCollection\",\n"
        "  \"crs\": {\n"
        "    \"type\": \"name\",\n"
        "    \"properties\": { \"name\": \"urn:ogc:def:crs:IAU_2015::49900\" }\n"
        "  },\n"
        "  \"features\": [\n"
        "    {\n"
        "      \"type\": \"Feature\",\n"
        "      \"properties\": {\n"
        "        \"total_distance_m\": %.2f,\n"
        "        \"total_duration_s\": %.2f,\n"
        "        \"total_cost\": %.2f,\n"
        "        \"max_slope_deg\": %.2f,\n"
        "        \"cumulative_elevation_gain_m\": %.2f,\n"
        "        \"total_radiation_dose_msv\": %.6f,\n"
        "        \"min_temp_c\": %.2f,\n"
        "        \"max_temp_c\": %.2f,\n"
        "        \"waypoints_count\": %zu\n"
        "      },\n"
        "      \"geometry\": {\n"
        "        \"type\": \"LineString\",\n"
        "        \"coordinates\": [\n",
        path->total_distance_m,
        path->total_duration_s,
        path->total_cost,
        path->max_slope_deg,
        path->cumulative_elevation_gain_m,
        path->total_radiation_dose_msv,
        path->min_temp_c,
        path->max_temp_c,
        path->count
    );

    if (written < 0 || (size_t)written >= max_len - offset) return -1;
    offset += (size_t)written;

    for (size_t i = 0; i < path->count; i++) {
        const char *comma = (i + 1 < path->count) ? "," : "";
        written = snprintf(buffer + offset, max_len - offset,
            "          [%.7f, %.7f, %.2f, %.1f, %.4f, %.2f]%s\n",
            path->points[i].lon_deg,
            path->points[i].lat_deg,
            path->points[i].elevation_m,
            path->points[i].elapsed_time_s,
            path->points[i].dose_rate_msv_day,
            path->points[i].temp_c,
            comma
        );
        if (written < 0 || (size_t)written >= max_len - offset) return -1;
        offset += (size_t)written;
    }

    written = snprintf(buffer + offset, max_len - offset,
        "        ]\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n"
    );

    if (written < 0 || (size_t)written >= max_len - offset) return -1;
    offset += (size_t)written;

    return (int)offset;
}

bool export_path_to_geojson_file(const char *filename, const PathResult *path) {
    FILE *f = fopen(filename, "w");
    if (!f) return false;

    fprintf(f,
        "{\n"
        "  \"type\": \"FeatureCollection\",\n"
        "  \"crs\": {\n"
        "    \"type\": \"name\",\n"
        "    \"properties\": { \"name\": \"urn:ogc:def:crs:IAU_2015::49900\" }\n"
        "  },\n"
        "  \"features\": [\n"
        "    {\n"
        "      \"type\": \"Feature\",\n"
        "      \"properties\": {\n"
        "        \"total_distance_m\": %.2f,\n"
        "        \"total_duration_s\": %.2f,\n"
        "        \"total_cost\": %.2f,\n"
        "        \"max_slope_deg\": %.2f,\n"
        "        \"cumulative_elevation_gain_m\": %.2f,\n"
        "        \"total_radiation_dose_msv\": %.6f,\n"
        "        \"min_temp_c\": %.2f,\n"
        "        \"max_temp_c\": %.2f,\n"
        "        \"waypoints_count\": %zu\n"
        "      },\n"
        "      \"geometry\": {\n"
        "        \"type\": \"LineString\",\n"
        "        \"coordinates\": [\n",
        path->total_distance_m,
        path->total_duration_s,
        path->total_cost,
        path->max_slope_deg,
        path->cumulative_elevation_gain_m,
        path->total_radiation_dose_msv,
        path->min_temp_c,
        path->max_temp_c,
        path->count
    );

    for (size_t i = 0; i < path->count; i++) {
        fprintf(f, "          [%.7f, %.7f, %.2f, %.1f, %.4f, %.2f]%s\n",
            path->points[i].lon_deg,
            path->points[i].lat_deg,
            path->points[i].elevation_m,
            path->points[i].elapsed_time_s,
            path->points[i].dose_rate_msv_day,
            path->points[i].temp_c,
            (i + 1 < path->count) ? "," : "");
    }

    fprintf(f,
        "        ]\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n"
    );

    fclose(f);
    return true;
}
