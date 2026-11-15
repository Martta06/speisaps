/*
 * pathfinder_astar.c — Implementación de A* con Min-Heap sobre ventana local del DEM.
 */

#include "pathfinder_astar.h"
#include "edge_cost.h"
#include "mars_geo.h"
#include "terrain_cost.h"
#include "radiation_bridge.h"
#include "../radiation_data/mars_env.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SQRT2 1.4142135623730951

/* Estructura para el heap de prioridad */
typedef struct {
    int x;
    int y;
    double f_score;
} HeapNode;

typedef struct {
    HeapNode *nodes;
    size_t size;
    size_t capacity;
} MinHeap;

static MinHeap* heap_create(size_t initial_cap) {
    MinHeap *h = (MinHeap*)malloc(sizeof(MinHeap));
    h->size = 0;
    h->capacity = initial_cap;
    h->nodes = (HeapNode*)malloc(initial_cap * sizeof(HeapNode));
    return h;
}

static void heap_free(MinHeap *h) {
    if (!h) return;
    free(h->nodes);
    free(h);
}

static void heap_push(MinHeap *h, int x, int y, double f_score) {
    if (h->size >= h->capacity) {
        h->capacity *= 2;
        h->nodes = (HeapNode*)realloc(h->nodes, h->capacity * sizeof(HeapNode));
    }
    size_t i = h->size++;
    h->nodes[i] = (HeapNode){ x, y, f_score };

    while (i > 0) {
        size_t p = (i - 1) / 2;
        if (h->nodes[p].f_score <= h->nodes[i].f_score) break;
        HeapNode tmp = h->nodes[p];
        h->nodes[p] = h->nodes[i];
        h->nodes[i] = tmp;
        i = p;
    }
}

static bool heap_pop(MinHeap *h, HeapNode *out_node) {
    if (h->size == 0) return false;
    *out_node = h->nodes[0];
    h->nodes[0] = h->nodes[--h->size];

    size_t i = 0;
    while (2 * i + 1 < h->size) {
        size_t left = 2 * i + 1;
        size_t right = 2 * i + 2;
        size_t smallest = left;

        if (right < h->size && h->nodes[right].f_score < h->nodes[left].f_score) {
            smallest = right;
        }
        if (h->nodes[i].f_score <= h->nodes[smallest].f_score) break;

        HeapNode tmp = h->nodes[i];
        h->nodes[i] = h->nodes[smallest];
        h->nodes[smallest] = tmp;
        i = smallest;
    }
    return true;
}

/* Vecindad de 8 direcciones */
static const int DX[8] = { 1, -1,  0,  0, 1, -1,  1, -1 };
static const int DY[8] = { 0,  0,  1, -1, 1, -1, -1,  1 };
static const double DIST_STEP[8] = { 1.0, 1.0, 1.0, 1.0, SQRT2, SQRT2, SQRT2, SQRT2 };

/* Función de velocidad efectiva considerando la pendiente angular */
static inline double traversal_time_step(double dist_m, double slope_deg, double v_nom_ms) {
    if (v_nom_ms <= 0.0) v_nom_ms = 1.0;
    /* Reducción exponencial suave de velocidad con pendiente para trajes/rovers */
    double slope_rad = slope_deg * (M_PI / 180.0);
    double factor = exp(1.5 * sin(slope_rad)); /* subidas ralentizan */
    return (dist_m / v_nom_ms) * factor;
}

bool astar_find_path(const DemContext              *ctx,
                     double                         start_lon,
                     double                         start_lat,
                     double                         goal_lon,
                     double                         goal_lat,
                     double                         start_sol_hour,
                     int                            step_meters,
                     const CostWeights             *weights,
                     const TraversalLimits         *limits,
                     const DynamicEnvironmentState *env,
                     PathResult                    *out_path)
{
    if (!ctx || !weights || !limits || !out_path) return false;
    if (step_meters < 1) step_meters = 1;

    /* Sin estado dinámico: calma y sin blindaje (conservador) */
    DynamicEnvironmentState env_local = env_state_calm(MODE_FOOT_EVA);
    if (env) env_local = *env;
    const float shielding = (float)env_local.vehicle_shielding;
    /* Velocidad nominal: ~1.25 m/s en Rover vs ~0.50 m/s a pie en traje EVA */
    const double v_nom_ms = (shielding < 0.95f) ? 1.25 : 0.50;

    int sx, sy, gx, gy;
    dem_geo_to_pixel(ctx, start_lon, start_lat, &sx, &sy);
    dem_geo_to_pixel(ctx, goal_lon,  goal_lat,  &gx, &gy);

    if (!dem_in_bounds(ctx, sx, sy) || !dem_in_bounds(ctx, gx, gy)) {
        fprintf(stderr, "astar: inicio o meta fuera de límites del DEM\n");
        return false;
    }

    /* Alinear coordenadas a la rejilla de paso step_meters */
    int step = step_meters;
    sx = (sx / step) * step;
    sy = (sy / step) * step;
    gx = (gx / step) * step;
    gy = (gy / step) * step;

    /* Definir caja englobante (bounding box) local con margen en píxeles (~1.0 m/px en DEM) */
    int base_margin_px = 20 * step;
    int storm_margin_px = 0;
    if (env_local.dust_storm_active && env_local.storm_radius_m > 0.0) {
        /* Garantizar espacio suficiente (~2.2x radio) para rodear el frente predicho sin salirse de la ventana */
        storm_margin_px = (int)ceil(env_local.storm_radius_m * 2.2);
    }
    if (env_local.spe_active && env_local.spe_storm_rate_uSv_h > 0.0) {
        /* Margen ampliado (~1.5 km) durante SPE para permitir buscar cañones, escarpes y depresiones */
        int spe_margin_px = 1500;
        if (spe_margin_px > storm_margin_px) storm_margin_px = spe_margin_px;
    }
    int margin_px = (storm_margin_px > base_margin_px) ? storm_margin_px : base_margin_px;
    int min_x = (sx < gx ? sx : gx) - margin_px;
    int max_x = (sx > gx ? sx : gx) + margin_px;
    int min_y = (sy < gy ? sy : gy) - margin_px;
    int max_y = (sy > gy ? sy : gy) + margin_px;

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= (int)ctx->width)  max_x = (int)ctx->width - 1;
    if (max_y >= (int)ctx->height) max_y = (int)ctx->height - 1;

    /* Alinear límites a múltiplos del paso */
    min_x = (min_x / step) * step;
    min_y = (min_y / step) * step;
    max_x = (max_x / step) * step;
    max_y = (max_y / step) * step;

    int grid_w = (max_x - min_x) / step + 1;
    int grid_h = (max_y - min_y) / step + 1;
    size_t total_cells = (size_t)grid_w * grid_h;

    double *g_score = (double*)malloc(total_cells * sizeof(double));
    double *time_score = (double*)malloc(total_cells * sizeof(double));
    int *came_from_x = (int*)malloc(total_cells * sizeof(int));
    int *came_from_y = (int*)malloc(total_cells * sizeof(int));
    unsigned char *closed = (unsigned char*)calloc(total_cells, 1);

    for (size_t i = 0; i < total_cells; i++) {
        g_score[i] = INFINITY;
        time_score[i] = INFINITY;
        came_from_x[i] = -1;
        came_from_y[i] = -1;
    }

    #define CELL_IDX(x, y) ((((y) - min_y) / step) * grid_w + (((x) - min_x) / step))

    MinHeap *open_set = heap_create(8192);

    size_t start_idx = CELL_IDX(sx, sy);
    g_score[start_idx] = 0.0;
    time_score[start_idx] = 0.0;

    /* Heurística admisible euclídea (distancia en metros multiplicada por peso base) */
    double h_start = hypot(gx - sx, gy - sy) * weights->w_dist;
    heap_push(open_set, sx, sy, h_start);

    float z_start_val = -4500.0f, z_goal_val = -4500.0f;
    dem_get_elevation(ctx, sx, sy, &z_start_val);
    dem_get_elevation(ctx, gx, gy, &z_goal_val);
    double route_dx = (double)(gx - sx);
    double route_dy = (double)(gy - sy);
    double route_len_sq = route_dx * route_dx + route_dy * route_dy;

    bool reached = false;
    HeapNode current;

    while (heap_pop(open_set, &current)) {
        if (current.x == gx && current.y == gy) {
            reached = true;
            break;
        }

        size_t curr_idx = CELL_IDX(current.x, current.y);
        if (closed[curr_idx]) continue;
        closed[curr_idx] = 1;

        float z_curr;
        if (!dem_get_elevation(ctx, current.x, current.y, &z_curr)) continue;
        double t_curr = time_score[curr_idx];

        for (int d = 0; d < 8; d++) {
            int nx = current.x + DX[d] * step;
            int ny = current.y + DY[d] * step;

            if (nx < min_x || nx > max_x || ny < min_y || ny > max_y) continue;

            size_t n_idx = CELL_IDX(nx, ny);
            if (closed[n_idx]) continue;

            float z_next;
            if (!dem_get_elevation(ctx, nx, ny, &z_next)) continue;

            double step_dist = DIST_STEP[d] * (double)step; /* ej. 10.0 m o 14.14 m */
            double dz = (double)(z_next - z_curr);
            double slope = mars_slope_deg(dz, step_dist);

            /* Tiempo de cruce de este tramo (Rover ~1.25 m/s vs EVA a pie ~0.50 m/s) */
            double dt = traversal_time_step(step_dist, slope, v_nom_ms);
            double tentative_t = t_curr + dt;

            /* Hora solar local estimada al atravesar este tramo */
            double sol_hour_arrival = start_sol_hour + (tentative_t / 3600.0);

            /* Tasa SPE vigente en el instante de paso (0 si fuera de la ventana) */
            float spe_rate = (float)env_spe_rate_at(&env_local, tentative_t);
            double current_dose_rate = radiation_query_dose_rate_shielded(
                (float)sol_hour_arrival,
                nx,
                ny,
                shielding,
                spe_rate
            );
            double effective_dose_for_cost = current_dose_rate;
            if (spe_rate > 0.0f) {
                /*
                 * En tormenta solar (SPE), amplificamos el contraste de refugio topográfico:
                 * 1) Obstrucción del horizonte en la LUT (1 - svf_eff): escarpes, paredes de cráter y cañones.
                 * 2) Columna atmosférica / depresión local: cotas más bajas respecto a la línea A->B
                 *    ofrecen refugio geométrico y mayor espesor de CO2 frente a crestas expuestas.
                 */
                double calm_dose = radiation_query_dose_rate_shielded(
                    (float)sol_hour_arrival, nx, ny, shielding, 0.0f
                );
                double spe_delta = current_dose_rate - calm_dose;
                double max_spe_delta = (double)spe_rate * (double)shielding * 24.0 / 1000.0;
                double svf_eff = (max_spe_delta > 1e-6) ? (spe_delta / max_spe_delta) : 1.0;
                double obstructed = (svf_eff < 1.0) ? (1.0 - svf_eff) : 0.0;

                double t_proj = (route_len_sq > 1.0)
                    ? (((double)(nx - sx) * route_dx + (double)(ny - sy) * route_dy) / route_len_sq)
                    : 0.5;
                if (t_proj < 0.0) t_proj = 0.0;
                if (t_proj > 1.0) t_proj = 1.0;
                double z_line = (double)z_start_val + t_proj * (double)(z_goal_val - z_start_val);
                double dz_rel = (double)z_next - z_line;

                double topo_exposure = exp(-55.0 * obstructed);
                double elev_exposure = 1.0 + 0.06 * dz_rel;
                if (elev_exposure < 0.20) elev_exposure = 0.20;
                if (elev_exposure > 3.50) elev_exposure = 3.50;

                effective_dose_for_cost = calm_dose + spe_delta * 4.5 * topo_exposure * elev_exposure;
            }

            /* Calcular temperatura atmosférica con get_mars_temperature() de mars_env.c */
            MarsTemperature mt = get_mars_temperature(
                (float)sol_hour_arrival,
                z_next,
                190.0f,   /* T_min = 190 K (-83°C) a nivel base */
                280.0f,   /* T_max = 280 K (+6.8°C) a mediodía */
                -4500.0f  /* Cota base de Gale Crater en metros */
            );

            /* Calcular micro-rugosidad dinámica desde el DEM en ventana 3x3 */
            double rough = 0.0;
            dem_get_roughness(ctx, nx, ny, &rough);

            /* Evaluar Tormenta de Arena Dinámica en el instante tentative_t */
            double storm_penalty = 0.0;
            if (env_local.dust_storm_active && env_local.storm_radius_m > 0.0) {
                double n_lon = 0.0, n_lat = 0.0;
                dem_pixel_to_geo(ctx, nx, ny, &n_lon, &n_lat);

                /* Desplazamiento lineal del centro de la tormenta en el instante tentative_t */
                double lat_ref_rad = env_local.storm_center_lat * (M_PI / 180.0);
                double m_per_deg_lat = MARS_RADIUS_M * (M_PI / 180.0);
                double m_per_deg_lon = MARS_RADIUS_M * cos(lat_ref_rad) * (M_PI / 180.0);

                /*
                 * Evaluamos TANTO la posición actual (t=0, círculo visible en pantalla)
                 * COMO la posición futura predicha en la dirección de avance de la tormenta
                 * (acotada a 0.85 * R por delante del frente para crear un escudo de barlovento
                 * sin despegarse nunca del círculo dibujado en el mapa).
                 */
                double shift_east_m  = env_local.storm_vel_east_ms  * tentative_t;
                double shift_north_m = env_local.storm_vel_north_ms * tentative_t;
                double shift_norm_m  = hypot(shift_east_m, shift_north_m);
                double max_lead_m    = env_local.storm_radius_m * 0.85;
                if (shift_norm_m > max_lead_m && shift_norm_m > 1e-6) {
                    double scale = max_lead_m / shift_norm_m;
                    shift_east_m  *= scale;
                    shift_north_m *= scale;
                }

                double storm_lon_t = env_local.storm_center_lon + shift_east_m  / m_per_deg_lon;
                double storm_lat_t = env_local.storm_center_lat + shift_north_m / m_per_deg_lat;

                double d_now  = mars_haversine_m(n_lat, n_lon, env_local.storm_center_lat, env_local.storm_center_lon);
                double d_pred = mars_haversine_m(n_lat, n_lon, storm_lat_t, storm_lon_t);
                double d_m    = (d_now < d_pred) ? d_now : d_pred;

                double r_storm = env_local.storm_radius_m * 1.12; /* Margen exterior 112% R */
                double r_core  = env_local.storm_radius_m * 0.55; /* Núcleo bloqueado 55% R */

                if (d_m < r_core) {
                    /*
                     * Núcleo denso (< 55% del radio): bloqueo duro (INFINITY),
                     * SALVO que la tormenta esté encima del explorador y este paso
                     * se esté alejando del centro (maniobra de evacuación del núcleo).
                     */
                    double c_lon = 0.0, c_lat = 0.0;
                    dem_pixel_to_geo(ctx, current.x, current.y, &c_lon, &c_lat);
                    double dc_now  = mars_haversine_m(c_lat, c_lon, env_local.storm_center_lat, env_local.storm_center_lon);
                    double dc_pred = mars_haversine_m(c_lat, c_lon, storm_lat_t, storm_lon_t);
                    double d_curr  = (dc_now < dc_pred) ? dc_now : dc_pred;
                    if (d_curr < r_core && d_m > d_curr) {
                        /* Evacuación de emergencia hacia el exterior del núcleo */
                        storm_penalty = (double)env_local.storm_severity_penalty * 15.0;
                    } else {
                        storm_penalty = INFINITY;
                    }
                } else if (d_m < r_storm) {
                    /*
                     * Periferia (55% a 112% del radio): perfil cuadrático-exponencial pronunciado.
                     */
                    double u = (r_storm - d_m) / (r_storm - r_core); /* u en (0, 1] */
                    storm_penalty = (double)env_local.storm_severity_penalty
                                  * (u * u) * exp(2.0 * u);
                }
            }

            /* Evaluar parámetros estrictamente reales con la función de coste limpia */
            EdgeParams ep = {
                .delta_s_m          = step_dist,
                .delta_z_m          = dz,
                .slope_deg          = slope,
                .roughness          = rough,
                .elapsed_time_s     = tentative_t,
                .dose_rate_msv_day  = effective_dose_for_cost,
                .temp_c             = (double)mt.temp_c,
                .speed_ms           = 1.0,
                .dust_storm_penalty = storm_penalty
            };

            double step_cost = edge_cost(&ep, weights, limits);
            if (!isfinite(step_cost)) continue;

            double tentative_g = g_score[curr_idx] + step_cost;

            if (tentative_g < g_score[n_idx]) {
                came_from_x[n_idx] = current.x;
                came_from_y[n_idx] = current.y;
                g_score[n_idx] = tentative_g;
                time_score[n_idx] = tentative_t;

                double h = hypot(gx - nx, gy - ny) * weights->w_dist;
                heap_push(open_set, nx, ny, tentative_g + h);
            }
        }
    }

    if (!reached) {
        heap_free(open_set);
        free(g_score);
        free(time_score);
        free(came_from_x);
        free(came_from_y);
        free(closed);
        return false;
    }

    /* Reconstruir el camino óptimo */
    size_t count = 0;
    int cx = gx, cy = gy;
    while (cx != -1 && cy != -1) {
        count++;
        if (cx == sx && cy == sy) break;
        size_t idx = CELL_IDX(cx, cy);
        int px = came_from_x[idx];
        int py = came_from_y[idx];
        cx = px;
        cy = py;
    }

    out_path->points = (PathPoint*)malloc(count * sizeof(PathPoint));
    out_path->count = count;
    out_path->total_cost = g_score[CELL_IDX(gx, gy)];
    out_path->total_duration_s = time_score[CELL_IDX(gx, gy)];
    out_path->total_distance_m = 0.0;
    out_path->max_slope_deg = 0.0;
    out_path->cumulative_elevation_gain_m = 0.0;
    out_path->total_radiation_dose_msv = 0.0;
    out_path->peak_dose_rate_msv_day = 0.0;
    out_path->spe_active = env_local.spe_active && env_local.spe_storm_rate_uSv_h > 0.0;
    out_path->min_temp_c = 100.0;
    out_path->max_temp_c = -273.15;

    cx = gx; cy = gy;
    for (size_t i = count; i > 0; i--) {
        size_t idx = i - 1;
        double lon, lat;
        dem_pixel_to_geo(ctx, cx, cy, &lon, &lat);
        float z = 0.0f;
        dem_get_elevation(ctx, cx, cy, &z);
        double t_arr = time_score[CELL_IDX(cx, cy)];
        double cur_h = start_sol_hour + (t_arr / 3600.0);
        float spe_val = (float)env_spe_rate_at(&env_local, t_arr);
        double cur_rate = radiation_query_dose_rate_shielded(
            (float)cur_h,
            cx,
            cy,
            shielding,
            spe_val
        );
        MarsTemperature mt_step = get_mars_temperature(
            (float)cur_h,
            z,
            190.0f,
            280.0f,
            -4500.0f
        );

        out_path->points[idx] = (PathPoint){
            .lon_deg           = lon,
            .lat_deg           = lat,
            .elevation_m       = (double)z,
            .elapsed_time_s    = t_arr,
            .dose_rate_msv_day = cur_rate,
            .temp_c            = (double)mt_step.temp_c
        };

        if (cx == sx && cy == sy) break;
        size_t c_idx = CELL_IDX(cx, cy);
        cx = came_from_x[c_idx];
        cy = came_from_y[c_idx];
    }

    /* Calcular estadísticas de la ruta reconstruida */
    for (size_t i = 1; i < count; i++) {
        double dz = out_path->points[i].elevation_m - out_path->points[i - 1].elevation_m;
        double ds = mars_haversine_m(out_path->points[i - 1].lat_deg, out_path->points[i - 1].lon_deg,
                                     out_path->points[i].lat_deg,     out_path->points[i].lon_deg);
        out_path->total_distance_m += ds;
        if (dz > 0.0) out_path->cumulative_elevation_gain_m += dz;

        double slope = mars_slope_deg(dz, ds > 0.001 ? ds : 1.0);
        if (slope > out_path->max_slope_deg) {
            out_path->max_slope_deg = slope;
        }

        /* Temperatura extrema en ruta */
        if (out_path->points[i].temp_c < out_path->min_temp_c) {
            out_path->min_temp_c = out_path->points[i].temp_c;
        }
        if (out_path->points[i].temp_c > out_path->max_temp_c) {
            out_path->max_temp_c = out_path->points[i].temp_c;
        }

        /* Acumulación de dosis real: dt_tramo * rate */
        double dt_segment = out_path->points[i].elapsed_time_s - out_path->points[i - 1].elapsed_time_s;
        double seg_rate = out_path->points[i].dose_rate_msv_day;
        out_path->total_radiation_dose_msv += seg_rate * (dt_segment / 86400.0);
        if (seg_rate > out_path->peak_dose_rate_msv_day) {
            out_path->peak_dose_rate_msv_day = seg_rate;
        }
    }

    heap_free(open_set);
    free(g_score);
    free(time_score);
    free(came_from_x);
    free(came_from_y);
    free(closed);

    return true;
}

void free_path_result(PathResult *path) {
    if (!path) return;
    free(path->points);
    path->points = NULL;
    path->count = 0;
}
