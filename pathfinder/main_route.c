/*
 * main_route.c — Punto de entrada CLI para calcular rutas óptimas en Marte.
 *
 * Lee el archivo Gale_DEM_SMG_1m.tif, busca la ruta óptima entre dos coordenadas
 * y genera el GeoJSON de salida compatible con el visor OpenLayers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dem_reader.h"
#include "params.h"
#include "pathfinder_astar.h"
#include "geojson_export.h"
#include "radiation_bridge.h"

int main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printf("============================================================\n");
        printf("  Martian Pathfinder CLI — Cráter Gale (Speisaps)\n");
        printf("============================================================\n");
        printf("Sintaxis:\n");
        printf("  %s <start_lon> <start_lat> <goal_lon> <goal_lat> [hora_lmst] [is_rover] [step_m] [out_geojson] [dem_tif]\n\n", argv[0]);
        printf("Parámetros:\n");
        printf("  1. start_lon:   Longitud origen en grados (ej. 137.350000)\n");
        printf("  2. start_lat:   Latitud origen en grados (ej. -4.500000)\n");
        printf("  3. goal_lon:    Longitud destino en grados (ej. 137.469662)\n");
        printf("  4. goal_lat:    Latitud destino en grados (ej. -4.619293)\n");
        printf("  5. hora_lmst:   Hora solar local marciana (0.0 a 24.0, por defecto 9.0)\n");
        printf("  6. is_rover:    0 = EVA a pie (astronauta), 1 = Rover (por defecto 0)\n");
        printf("  7. step_m:      Resolución: 10 (rápido, 0.2s-2s) o 1 (táctico 1m) (por defecto 10)\n");
        printf("  8. out_geojson: Archivo de salida (por defecto route_output.geojson)\n");
        printf("  9. dem_tif:     Ruta al DEM (por defecto ../Gale_DEM_SMG_1m.tif)\n");
        printf("============================================================\n");
        return 0;
    }

    const char *dem_file = "../Gale_DEM_SMG_1m.tif";
    const char *out_geojson = "route_output.geojson";

    /* Puntos por defecto en la zona plana central de Gale Crater */
    double start_lon = 137.4250;
    double start_lat = -4.5800;
    double goal_lon  = 137.4270;
    double goal_lat  = -4.5820;
    double start_sol_hour = 9.0; /* 09:00 LMST por defecto */
    int is_rover = 0;            /* 0 = EVA a pie, 1 = Rover */
    int step_meters = 10;        /* 10 m por defecto para alta velocidad, 1 m para táctico */

    if (argc >= 5) {
        start_lon = atof(argv[1]);
        start_lat = atof(argv[2]);
        goal_lon  = atof(argv[3]);
        goal_lat  = atof(argv[4]);
    }
    if (argc >= 6) {
        start_sol_hour = atof(argv[5]);
    }
    if (argc >= 7) {
        is_rover = atoi(argv[6]);
    }
    if (argc >= 8) {
        step_meters = atoi(argv[7]);
    }
    if (argc >= 9) {
        out_geojson = argv[8];
    }
    if (argc >= 10) {
        dem_file = argv[9];
    }

    printf("Iniciando Martian Pathfinder (Gale Crater)...\n");
    printf("DEM: %s\n", dem_file);
    printf("Modo:          %s\n", is_rover ? "ROVER (Curiosity/Perseverance)" : "FOOT EVA (Astronauta)");
    printf("Resolución:    1 punto cada %d metros\n", step_meters);
    printf("Origen:        Lon=%.6f, Lat=%.6f\n", start_lon, start_lat);
    printf("Destino:       Lon=%.6f, Lat=%.6f\n", goal_lon,  goal_lat);
    printf("Hora partida:  %.2f LMST (Sol hour)\n", start_sol_hour);

    DemContext ctx;
    if (!dem_open(dem_file, &ctx)) {
        fprintf(stderr, "Error al abrir el DEM marciano.\n");
        return 1;
    }

    /* Inicializar el modelo de radiación con la LUT y la curva 24h del colaborador */
    const char *lut_bin = "../radiation_data/gale_shielding_lut.bin";
    const char *base_bin = "../radiation_data/rad_baseline_24h.bin";
    if (radiation_init(lut_bin, base_bin)) {
        printf("Modelo de radiación: cargado desde radiation_data/ (LUT 4010x3600 + Curva 24h)\n");
    } else {
        printf("Aviso: usando fallback para modelo de radiación\n");
    }

    TraversalLimits limits = is_rover ? limits_default_rover() : limits_default_foot_eva();
    CostWeights weights    = is_rover ? weights_default_rover() : weights_default_foot_eva();

    PathResult path;
    printf("Buscando ruta óptima con A* espacio-temporal...\n");
    if (!astar_find_path(&ctx, start_lon, start_lat, goal_lon, goal_lat, start_sol_hour, step_meters, &weights, &limits, &path)) {
        fprintf(stderr, "No se encontró una ruta viable cumpliendo las restricciones marcianas.\n");
        dem_close(&ctx);
        return 2;
    }

    printf("Ruta encontrada exitosamente!\n");
    printf("  Puntos en el trayecto: %zu\n", path.count);
    printf("  Distancia total:       %.2f m\n", path.total_distance_m);
    printf("  Duración estimada:     %.2f s (%.2f min)\n", path.total_duration_s, path.total_duration_s / 60.0);
    printf("  Rango de temperatura:  %.1f °C a %.1f °C\n", path.min_temp_c, path.max_temp_c);
    printf("  Dosis radiación acum.: %.6f mSv\n", path.total_radiation_dose_msv);
    printf("  Coste total:           %.2f\n", path.total_cost);
    printf("  Pendiente máxima:      %.2f°\n", path.max_slope_deg);
    printf("  Desnivel positivo:     %.2f m\n", path.cumulative_elevation_gain_m);

    if (export_path_to_geojson_file(out_geojson, &path)) {
        printf("Ruta GeoJSON exportada a: %s\n", out_geojson);
    } else {
        fprintf(stderr, "Error al exportar archivo GeoJSON.\n");
    }

    free_path_result(&path);
    radiation_cleanup();
    dem_close(&ctx);
    return 0;
}
