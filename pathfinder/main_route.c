/*
 * main_route.c — Punto de entrada CLI para calcular rutas óptimas en Marte.
 *
 * Lee el archivo Gale_DEM_SMG_1m.tif, busca la ruta óptima entre dos coordenadas
 * y genera el GeoJSON de salida compatible con el visor OpenLayers.
 */

#include <stdio.h>
#include <stdlib.h>
#include "dem_reader.h"
#include "params.h"
#include "pathfinder_astar.h"
#include "geojson_export.h"

int main(int argc, char **argv) {
    const char *dem_file = "../Gale_DEM_SMG_1m.tif";
    const char *out_geojson = "route_output.geojson";

    /* Puntos por defecto en la zona plana central de Gale Crater */
    double start_lon = 137.4250;
    double start_lat = -4.5800;
    double goal_lon  = 137.4270;
    double goal_lat  = -4.5820;

    if (argc >= 5) {
        start_lon = atof(argv[1]);
        start_lat = atof(argv[2]);
        goal_lon  = atof(argv[3]);
        goal_lat  = atof(argv[4]);
    }
    if (argc >= 6) {
        out_geojson = argv[5];
    }
    if (argc >= 7) {
        dem_file = argv[6];
    }

    printf("Iniciando Martian Pathfinder (Gale Crater)...\n");
    printf("DEM: %s\n", dem_file);
    printf("Origen:  Lon=%.6f, Lat=%.6f\n", start_lon, start_lat);
    printf("Destino: Lon=%.6f, Lat=%.6f\n", goal_lon,  goal_lat);

    DemContext ctx;
    if (!dem_open(dem_file, &ctx)) {
        fprintf(stderr, "Error al abrir el DEM marciano.\n");
        return 1;
    }

    TraversalLimits limits = limits_default_foot_eva();
    CostWeights weights = weights_default_foot_eva();

    PathResult path;
    printf("Buscando ruta óptima con A*...\n");
    if (!astar_find_path(&ctx, start_lon, start_lat, goal_lon, goal_lat, &weights, &limits, &path)) {
        fprintf(stderr, "No se encontró una ruta viable cumpliendo las restricciones marcianas.\n");
        dem_close(&ctx);
        return 2;
    }

    printf("Ruta encontrada exitosamente!\n");
    printf("  Puntos en el trayecto: %zu\n", path.count);
    printf("  Distancia total:       %.2f m\n", path.total_distance_m);
    printf("  Coste total:           %.2f\n", path.total_cost);
    printf("  Pendiente máxima:      %.2f°\n", path.max_slope_deg);
    printf("  Desnivel positivo:     %.2f m\n", path.cumulative_elevation_gain_m);

    if (export_path_to_geojson_file(out_geojson, &path)) {
        printf("Ruta GeoJSON exportada a: %s\n", out_geojson);
    } else {
        fprintf(stderr, "Error al exportar archivo GeoJSON.\n");
    }

    free_path_result(&path);
    dem_close(&ctx);
    return 0;
}
