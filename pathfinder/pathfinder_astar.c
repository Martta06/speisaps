/*
 * pathfinder_astar.c — Implementación de A* con Min-Heap sobre ventana local del DEM.
 */

#include "pathfinder_astar.h"
#include "edge_cost.h"
#include "mars_geo.h"
#include "terrain_cost.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

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

bool astar_find_path(const DemContext       *ctx,
                     double                  start_lon,
                     double                  start_lat,
                     double                  goal_lon,
                     double                  goal_lat,
                     const CostWeights      *weights,
                     const TraversalLimits  *limits,
                     PathResult             *out_path)
{
    if (!ctx || !weights || !limits || !out_path) return false;

    int sx, sy, gx, gy;
    dem_geo_to_pixel(ctx, start_lon, start_lat, &sx, &sy);
    dem_geo_to_pixel(ctx, goal_lon,  goal_lat,  &gx, &gy);

    if (!dem_in_bounds(ctx, sx, sy) || !dem_in_bounds(ctx, gx, gy)) {
        fprintf(stderr, "astar: inicio o meta fuera de límites del DEM\n");
        return false;
    }

    /* Definir caja englobante (bounding box) local con margen */
    int min_x = (sx < gx ? sx : gx) - 200;
    int max_x = (sx > gx ? sx : gx) + 200;
    int min_y = (sy < gy ? sy : gy) - 200;
    int max_y = (sy > gy ? sy : gy) + 200;

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= (int)ctx->width)  max_x = (int)ctx->width - 1;
    if (max_y >= (int)ctx->height) max_y = (int)ctx->height - 1;

    int grid_w = max_x - min_x + 1;
    int grid_h = max_y - min_y + 1;
    size_t total_cells = (size_t)grid_w * grid_h;

    double *g_score = (double*)malloc(total_cells * sizeof(double));
    int *came_from_x = (int*)malloc(total_cells * sizeof(int));
    int *came_from_y = (int*)malloc(total_cells * sizeof(int));
    unsigned char *closed = (unsigned char*)calloc(total_cells, 1);

    for (size_t i = 0; i < total_cells; i++) {
        g_score[i] = INFINITY;
        came_from_x[i] = -1;
        came_from_y[i] = -1;
    }

    #define CELL_IDX(x, y) (((y) - min_y) * grid_w + ((x) - min_x))

    MinHeap *open_set = heap_create(4096);

    size_t start_idx = CELL_IDX(sx, sy);
    g_score[start_idx] = 0.0;

    /* Heurística admisible euclídea (distancia en metros multiplicada por peso base) */
    double h_start = hypot(gx - sx, gy - sy) * weights->w_dist;
    heap_push(open_set, sx, sy, h_start);

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

        for (int d = 0; d < 8; d++) {
            int nx = current.x + DX[d];
            int ny = current.y + DY[d];

            if (nx < min_x || nx > max_x || ny < min_y || ny > max_y) continue;

            size_t n_idx = CELL_IDX(nx, ny);
            if (closed[n_idx]) continue;

            float z_next;
            if (!dem_get_elevation(ctx, nx, ny, &z_next)) continue;

            double step_dist = DIST_STEP[d]; /* 1.0 o ~1.414 metros */
            double dz = (double)(z_next - z_curr);
            double slope = mars_slope_deg(dz, step_dist);

            /* Evaluar parámetros con nuestra función modular */
            EdgeParams ep = {
                .delta_s_m         = step_dist,
                .delta_z_m         = dz,
                .slope_deg         = slope,
                .roughness_vrm     = 0.05,
                .rock_abundance    = 0.05,
                .thermal_inertia   = 200.0,
                .hazard_distance_m = 100.0,
                .max_rock_height_m = 0.05,
                .cell_class        = CELL_SAFE,
                .dose_rate_msv_day = 0.64,
                .wind_multiplier   = 0.0,
                .speed_ms          = 1.0
            };

            double step_cost = edge_cost(&ep, weights, limits);
            if (!isfinite(step_cost)) continue;

            double tentative_g = g_score[curr_idx] + step_cost;

            if (tentative_g < g_score[n_idx]) {
                came_from_x[n_idx] = current.x;
                came_from_y[n_idx] = current.y;
                g_score[n_idx] = tentative_g;

                double h = hypot(gx - nx, gy - ny) * weights->w_dist;
                heap_push(open_set, nx, ny, tentative_g + h);
            }
        }
    }

    if (!reached) {
        heap_free(open_set);
        free(g_score);
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
    out_path->total_distance_m = 0.0;
    out_path->max_slope_deg = 0.0;
    out_path->cumulative_elevation_gain_m = 0.0;

    cx = gx; cy = gy;
    for (size_t i = count; i > 0; i--) {
        size_t idx = i - 1;
        double lon, lat;
        dem_pixel_to_geo(ctx, cx, cy, &lon, &lat);
        float z = 0.0f;
        dem_get_elevation(ctx, cx, cy, &z);

        out_path->points[idx] = (PathPoint){
            .lon_deg     = lon,
            .lat_deg     = lat,
            .elevation_m = (double)z
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
    }

    heap_free(open_set);
    free(g_score);
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
