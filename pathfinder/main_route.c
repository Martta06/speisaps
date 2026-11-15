/*
 * main_route.c — Punto de entrada CLI para calcular rutas óptimas en Marte.
 *
 * Lee el archivo Gale_DEM_SMG_1m.tif, busca la ruta óptima entre dos coordenadas
 * y genera el GeoJSON de salida compatible con el visor OpenLayers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
        printf("  %s <start_lon> <start_lat> <goal_lon> <goal_lat> [hora_lmst] [is_rover] [step_m] [out_geojson] [dem_tif] [--spe <uSv/h>] [--spe-onset <s>] [--spe-end <s>] [--shield <f>]\n\n", argv[0]);
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
        printf("  --spe <uSv/h>:  Activa tormenta solar SPE extra a cielo abierto (ej. 150.0)\n");
        printf("  --spe-onset <s>: Segundos desde partida de inicio del SPE (defecto 0)\n");
        printf("  --spe-end <s>:   Segundos desde partida de fin del SPE (defecto infinito)\n");
        printf("  --shield <f>:    Factor de transmisión del vehículo (1.0 EVA, 0.85 Rover)\n");
        printf("  --dust-storm <lon> <lat> <radius_m> [vel_e_ms] [vel_n_ms] [penalty]:\n");
        printf("                   Activa tormenta de arena dinámica centrada en (lon, lat)\n");
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

    double spe_rate = 0.0, spe_onset = 0.0, spe_end = INFINITY, shield_override = -1.0;
    int dust_active = 0;
    double dust_lon = 0.0, dust_lat = 0.0, dust_rad_m = 0.0;
    double dust_ve = 0.0, dust_vn = 0.0;
    float dust_pen = 25.0f;

    /* Separar flags --xxx de los argumentos posicionales */
    const char *pos[16];
    int npos = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--spe") == 0 && i + 1 < argc) {
            spe_rate = atof(argv[++i]);
        } else if (strcmp(argv[i], "--spe-onset") == 0 && i + 1 < argc) {
            spe_onset = atof(argv[++i]);
        } else if (strcmp(argv[i], "--spe-end") == 0 && i + 1 < argc) {
            spe_end = atof(argv[++i]);
        } else if (strcmp(argv[i], "--shield") == 0 && i + 1 < argc) {
            shield_override = atof(argv[++i]);
        } else if (strcmp(argv[i], "--dust-storm") == 0 && i + 3 < argc) {
            dust_active = 1;
            dust_lon = atof(argv[++i]);
            dust_lat = atof(argv[++i]);
            dust_rad_m = atof(argv[++i]);
            if (i + 1 < argc && argv[i + 1][0] != '-') dust_ve  = atof(argv[++i]);
            if (i + 1 < argc && argv[i + 1][0] != '-') dust_vn  = atof(argv[++i]);
            if (i + 1 < argc && argv[i + 1][0] != '-') dust_pen = (float)atof(argv[++i]);
        } else if (npos < 16) {
            pos[npos++] = argv[i];
        }
    }

    if (npos >= 4) {
        start_lon = atof(pos[0]);
        start_lat = atof(pos[1]);
        goal_lon  = atof(pos[2]);
        goal_lat  = atof(pos[3]);
    }
    if (npos >= 5) start_sol_hour = atof(pos[4]);
    if (npos >= 6) is_rover = atoi(pos[5]);
    if (npos >= 7) step_meters = atoi(pos[6]);
    if (npos >= 8) out_geojson = pos[7];
    if (npos >= 9) dem_file = pos[8];

    DynamicEnvironmentState env = env_state_calm(is_rover ? MODE_ROVER : MODE_FOOT_EVA);
    if (spe_rate > 0.0) {
        env.spe_active = true;
        env.spe_storm_rate_uSv_h = spe_rate;
        env.spe_onset_s = spe_onset;
        env.spe_end_s = spe_end;
    }
    if (shield_override > 0.0) env.vehicle_shielding = shield_override;
    if (dust_active && dust_rad_m > 0.0) {
        env.dust_storm_active      = 1;
        env.storm_center_lon       = dust_lon;
        env.storm_center_lat       = dust_lat;
        env.storm_radius_m         = dust_rad_m;
        env.storm_vel_east_ms      = dust_ve;
        env.storm_vel_north_ms     = dust_vn;
        env.storm_severity_penalty = dust_pen;
    }

    printf("Iniciando Martian Pathfinder (Gale Crater)...\n");
    printf("DEM: %s\n", dem_file);
    printf("Modo:          %s\n", is_rover ? "ROVER (Curiosity/Perseverance)" : "FOOT EVA (Astronauta)");
    printf("Resolución:    1 punto cada %d metros\n", step_meters);
    printf("Origen:        Lon=%.6f, Lat=%.6f\n", start_lon, start_lat);
    printf("Destino:       Lon=%.6f, Lat=%.6f\n", goal_lon,  goal_lat);
    printf("Hora partida:  %.2f LMST (Sol hour)\n", start_sol_hour);
    printf("Blindaje veh.: x%.2f\n", env.vehicle_shielding);
    if (env.spe_active) {
        printf("TORMENTA SOLAR (SPE): +%.1f uSv/h a cielo abierto, desde t=%.0f s ",
               env.spe_storm_rate_uSv_h, env.spe_onset_s);
        if (isfinite(env.spe_end_s)) printf("hasta t=%.0f s\n", env.spe_end_s);
        else                         printf("(sin fin previsto)\n");
    } else {
        printf("SPE:           inactiva (solo GCR)\n");
    }
    if (env.dust_storm_active) {
        printf("TORMENTA DE ARENA: Centro=(%.5f, %.5f), Radio=%.0f m, Deriva=(%.1f, %.1f) m/s, Penalización=%.1f\n",
               env.storm_center_lon, env.storm_center_lat, env.storm_radius_m,
               env.storm_vel_east_ms, env.storm_vel_north_ms, env.storm_severity_penalty);
    }

    DemContext ctx;
    if (!dem_open(dem_file, &ctx) &&
        !dem_open("../../geotiff/Gale_DEM_SMG_1m.tif", &ctx) &&
        !dem_open("../radiation_data/hirise_dem.bin", &ctx) &&
        !dem_open("radiation_data/hirise_dem.bin", &ctx)) {
        fprintf(stderr, "Error al abrir el DEM marciano.\n");
        return 1;
    }

    /* Inicializar el modelo de radiación (LUT topográfica y curva diurna 24h) */
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
    if (!astar_find_path(&ctx, start_lon, start_lat, goal_lon, goal_lat, start_sol_hour, step_meters, &weights, &limits, &env, &path)) {
        fprintf(stderr, "No se encontró una ruta viable cumpliendo las restricciones marcianas.\n");
        radiation_cleanup();
        dem_close(&ctx);
        return 2;
    }

    printf("Ruta encontrada exitosamente!\n");
    printf("  Puntos en el trayecto: %zu\n", path.count);
    printf("  Distancia total:       %.2f m\n", path.total_distance_m);
    printf("  Duración estimada:     %.2f s (%.2f min)\n", path.total_duration_s, path.total_duration_s / 60.0);
    printf("  Rango de temperatura:  %.1f °C a %.1f °C\n", path.min_temp_c, path.max_temp_c);
    printf("  Dosis radiación acum.: %.6f mSv\n", path.total_radiation_dose_msv);
    printf("  Tasa de dosis máxima:  %.4f mSv/día (%.1f uSv/h)\n",
           path.peak_dose_rate_msv_day, path.peak_dose_rate_msv_day * 1000.0 / 24.0);
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
